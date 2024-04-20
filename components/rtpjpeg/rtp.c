#include "rtp.h"

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

__attribute__((unused)) static const char *TAG = "rtp";
static const ptrdiff_t HEADER_MIN_SZ = 12;

esp_err_t parse_rtp_packet(const uint8_t *buf, const ptrdiff_t sz, rtp_packet_t *out) {
    if (out == NULL || buf == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));

    if (sz < HEADER_MIN_SZ) {
        return ESP_ERR_INVALID_SIZE;
    }
    out->version = (buf[0] >> 6) & 0x03;
    if (out->version != 2) {
        return ESP_ERR_INVALID_VERSION;
    }

    out->padding = (buf[0] >> 5) & 0x01;
    out->extension = (buf[0] >> 4) & 0x01;
    out->csrc_count = buf[0] & 0x0F;
    out->marker = (buf[1] >> 7) & 0x01;
    out->payload_type = buf[1] & 0x7F;
    out->sequence_number = (buf[2] << 8) | buf[3];
    out->timestamp = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
    out->ssrc = (buf[8] << 24) | (buf[9] << 16) | (buf[10] << 8) | buf[11];

    assert(out->csrc_count <= 16);
    if (sz < 12 + out->csrc_count * 4) {
        return ESP_ERR_INVALID_SIZE;
    }
    for (uint8_t i = 0; i < out->csrc_count; i++) {
        const ptrdiff_t offs = HEADER_MIN_SZ + i * 4;
        out->csrc[i] =
            (buf[offs] << 24) | (buf[offs + 1] << 16) | (buf[offs + 2] << 8) | buf[offs + 3];
    }

    const ptrdiff_t parsed = HEADER_MIN_SZ + out->csrc_count * 4;
    assert(parsed <= sz);
    out->payload = (uint8_t *)&buf[parsed];
    out->payload_sz = sz - parsed;
    return ESP_OK;
}

esp_err_t partial_parse_rtp_packet(const uint8_t *buf, const ptrdiff_t sz,
                                   uint16_t *sequence_number_out, uint32_t *ssrc_out) {
    if (buf == NULL || sequence_number_out == NULL || ssrc_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (sz < HEADER_MIN_SZ) {
        return ESP_ERR_INVALID_SIZE;
    }
    const uint8_t version = (buf[0] >> 6) & 0x03;
    if (version != 2) {
        return ESP_ERR_INVALID_VERSION;
    }

    *sequence_number_out = (buf[2] << 8) | buf[3];
    *ssrc_out = (buf[8] << 24) | (buf[9] << 16) | (buf[10] << 8) | buf[11];

    return ESP_OK;
}

void rtp_packet_print(const rtp_packet_t p __attribute__((unused))) {
    ESP_LOGD(TAG,
             "RTP[v=%" PRIu8 " ext=%" PRIu8 " csrc=%" PRIu8 " mark=%" PRIu8 " pt=%" PRIu8
             " seq=%" PRIu16 " ts=%" PRIu32 " ssrc=%" PRIu32 "]",
             p.version, p.extension, p.csrc_count, p.marker, p.payload_type, p.sequence_number,
             p.timestamp, p.ssrc);
}

void init_rtp_jitbuf(const uint32_t ssrc, rtp_jitbuf_t *j) {
    assert(j != NULL);
    memset(j, 0, sizeof(*j));

    j->ssrc = ssrc;

    j->buf_top = -1;
    j->max_seq_out = -1;
}

static inline void rtp_jitbuf_logd(rtp_jitbuf_t *j __attribute__((unused))) {
#ifndef NDEBUG
#if CONFIG_LOG_MAXIMUM_LEVEL >= ESP_LOG_DEBUG
    char buf[CONFIG_RTP_JITBUF_CAP_N_PACKETS * 6 + 1] = {0};
    char *p = buf;

    for (int i = 0; i < CONFIG_RTP_JITBUF_CAP_N_PACKETS; i++) {
        if (i == j->buf_top) {
            p += snprintf(p, (buf + sizeof(buf) - p), "  |   ");
        } else {
            p += snprintf(p, (buf + sizeof(buf) - p), "      ");
        }
    }
    ESP_LOGD(TAG, "%s", buf);

    memset(buf, 0, sizeof(buf));
    p = buf;
    for (int i = 0; i < CONFIG_RTP_JITBUF_CAP_N_PACKETS; i++) {
        const ptrdiff_t sz = j->buf_szs[i];
        if (sz == 0) {
            p += snprintf(p, (buf + sizeof(buf) - p), "_____ ");
            continue;
        }

        uint16_t sequence_number = 0;
        uint32_t ssrc = 0;
        const esp_err_t err = partial_parse_rtp_packet(j->buf[i], sz, &sequence_number, &ssrc);
        assert(err == ESP_OK);

        p += snprintf(p, (buf + sizeof(buf) - p), "%5hu ", sequence_number);
    }

    ESP_LOGD(TAG, "%s", buf);
#endif
#endif
}

/**
 * Compare two sequence numbers, handling wraparounds.
 * See http://en.wikipedia.org/wiki/Serial_number_arithmetic for why this works.
 * Returns
 *  - A negative value if seq0 > seq1.
 *  - 0 if they are equal.
 *  - Positive if seq0 < seq1.
 */
static int32_t seqnum_compare(uint16_t seq0, uint16_t seq1) { return (int16_t)(seq1 - seq0); }

static int mod(int a, int b) { return (a % b + b) % b; }

esp_err_t rtp_jitbuf_feed(rtp_jitbuf_t *j, const uint8_t *buf, const ptrdiff_t sz) {
    uint16_t sequence_number = 0;
    uint32_t ssrc = 0;
    esp_err_t err = partial_parse_rtp_packet(buf, sz, &sequence_number, &ssrc);
    if (err != ESP_OK) {
        return err;
    }

    if (ssrc != j->ssrc) {
        return ESP_OK;
    }

    if (sz > CONFIG_RTP_JITBUF_CAP_PACKET_SIZE_BYTES) {
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGD(TAG, "->jitbuf state max_seq=%hu buf_top=%d", j->max_seq, j->buf_top);
    rtp_jitbuf_logd(j);
    ESP_LOGD(TAG, "->jitbuf new packet seq=%hu", sequence_number);

    // Buffer is empty -> place at start.
    if (j->buf_top < 0) {
        ESP_LOGD(TAG, "->jitbuf empty, place at start");
        assert(j->max_seq == 0);
        j->max_seq = sequence_number;
        j->buf_top = 0;
        assert(sz <= CONFIG_RTP_JITBUF_CAP_PACKET_SIZE_BYTES);
        memcpy(j->buf[j->buf_top], buf, sz);
        j->buf_szs[j->buf_top] = sz;
        return ESP_OK;
    }

    // Figure out where to place the new packet relative to the current newest one.
    const int32_t advance = seqnum_compare(j->max_seq, sequence_number);
    ESP_LOGD(TAG, "->jitbuf advance %" PRId32, advance);

    if (advance == 0) {
        // Duplicate, drop.
        return ESP_OK;
    }

    if (advance > 0) {
        for (int32_t i = 0; i < advance; i++) {
            j->buf_top = (j->buf_top + 1) % CONFIG_RTP_JITBUF_CAP_N_PACKETS;
            if (j->buf_szs[j->buf_top] > 0) {
                ESP_LOGD(TAG, "->jitbuf dropping packet from end of buffer at %d", j->buf_top);
                memset(j->buf[j->buf_top], 0, j->buf_szs[j->buf_top]);
                j->buf_szs[j->buf_top] = 0;
            }

            // No need to circle around more than that.
            if (i > CONFIG_RTP_JITBUF_CAP_N_PACKETS) {
                continue;
            }
        }

        ESP_LOGD(TAG, "->jitbuf place packet at %d", j->buf_top);
        j->max_seq = sequence_number;
        assert(sz <= CONFIG_RTP_JITBUF_CAP_PACKET_SIZE_BYTES);
        memcpy(j->buf[j->buf_top], buf, sz);
        assert(j->buf_szs[j->buf_top] == 0);
        j->buf_szs[j->buf_top] = sz;

        return ESP_OK;
    }

    if (advance <= -CONFIG_RTP_JITBUF_CAP_N_PACKETS) {
        // Too old, drop.
        ESP_LOGD(TAG,
                 "->jitbuf dropping incoming packet which is too late seq=%" PRIu16
                 " diff=%" PRId32,
                 sequence_number, advance);
        return ESP_OK;
    }

    // Place the packet somewhere in the middle.
    const int pos = mod(j->buf_top + advance, CONFIG_RTP_JITBUF_CAP_N_PACKETS);
    ESP_LOGD(TAG, "->jitbuf older packet seq=%" PRIu16 " diff=%" PRId32 " placing at %d",
             sequence_number, advance, pos);
    if (j->buf_szs[pos] != 0) {
        ESP_LOGD(TAG, "->jitbuf dropping older duplicate packet seq=%" PRIu16 " diff=%" PRId32,
                 sequence_number, advance);
        return ESP_OK;
    }
    assert(sz <= CONFIG_RTP_JITBUF_CAP_PACKET_SIZE_BYTES);
    memcpy(j->buf[pos], buf, sz);
    j->buf_szs[pos] = sz;

    return ESP_OK;
}

static int rtp_jitbuf_find_oldest_packet(rtp_jitbuf_t *j) {
    if (j->buf_top < 0) {
        return -1;
    }

    // We start searching one after buf_top.
    for (int i = 1; i <= CONFIG_RTP_JITBUF_CAP_N_PACKETS; i++) {
        const int pos = (j->buf_top + i) % CONFIG_RTP_JITBUF_CAP_N_PACKETS;
        if (j->buf_szs[pos] <= 0) {
            continue;
        }
        return pos;
    }

    // This should never happen, because we should have returned above already.
    assert(false);
    return -1;
}

static ptrdiff_t rtp_jitbuf_hand_out_buffer(rtp_jitbuf_t *j, const int pos,
                                            const uint16_t sequence_number, uint8_t *buf,
                                            const ptrdiff_t sz) {
    ESP_LOGV(TAG, "jitbuf-> hand out buffer %d len=%ld", pos, (long)j->buf_szs[pos]);
    j->max_seq_out = sequence_number;

    // Copy out.
    if (sz < CONFIG_RTP_JITBUF_CAP_PACKET_SIZE_BYTES) {
        return 0;
    }
    assert(pos >= 0 && pos < CONFIG_RTP_JITBUF_CAP_N_PACKETS);
    const ptrdiff_t ret = j->buf_szs[pos];
    memcpy(buf, j->buf[pos], j->buf_szs[pos]);

    // Delete buf slot.
    assert(j->buf_szs[pos] <= CONFIG_RTP_JITBUF_CAP_PACKET_SIZE_BYTES);
    memset(j->buf[pos], 0, j->buf_szs[pos]);
    j->buf_szs[pos] = 0;

    if (sequence_number == j->max_seq) {
        // We just handed out the last buffer.

        ESP_LOGD(TAG, "jitbuf-> is now empty, cleanup");

        j->max_seq = 0;
        j->buf_top = -1;

        for (int i = 0; i < CONFIG_RTP_JITBUF_CAP_N_PACKETS; i++) {
            assert(j->buf_szs[i] == 0);
        }
    }

    return ret;
}

ptrdiff_t rtp_jitbuf_retrieve(rtp_jitbuf_t *j, uint8_t *buf, const ptrdiff_t sz) {
    ESP_LOGD(TAG, "jitbuf-> state max_seq=%" PRIu16 " buf_top=%d max_seq_out=%" PRId32, j->max_seq,
             j->buf_top, j->max_seq_out);
    rtp_jitbuf_logd(j);

    const int pos = rtp_jitbuf_find_oldest_packet(j);
    if (pos < 0) {
        ESP_LOGV(TAG, "jitbuf-> is empty");
        return 0;
    }

    // Parse the candidate.
    uint16_t sequence_number = 0;
    uint32_t ssrc = 0;
    const esp_err_t err =
        partial_parse_rtp_packet(j->buf[pos], j->buf_szs[pos], &sequence_number, &ssrc);
    if (err != ESP_OK) {
        // This should never happen, we parse before placing them in buf.
        assert(false);
        return 0;
    }

    ESP_LOGV(TAG, "jitbuf-> consider packet at %d seq=%hu", pos, sequence_number);

    if (j->max_seq_out < 0 || sequence_number == j->max_seq_out + 1) {
        ESP_LOGD(TAG, "jitbuf-> hand out packet next in seq");
        return rtp_jitbuf_hand_out_buffer(j, pos, sequence_number, buf, sz);
    }

    // Buffer is full, hand out the last packet.
    if (mod(pos - 1, CONFIG_RTP_JITBUF_CAP_N_PACKETS) == j->buf_top) {
        ESP_LOGD(TAG, "jitbuf-> hand out packet because buffer is full pos=%d top=%d", pos,
                 j->buf_top);
        return rtp_jitbuf_hand_out_buffer(j, pos, sequence_number, buf, sz);
    }

    ESP_LOGV(TAG, "jitbuf-> nothing to hand out pos=%d top=%d +1=", pos, j->buf_top);
    return 0;
}
