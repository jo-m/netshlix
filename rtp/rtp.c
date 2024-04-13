#include "rtp.h"

#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "rtp";
static const ptrdiff_t HEADER_MIN_SZ = 12;

esp_err_t parse_rtp_packet(const uint8_t *buf, const ptrdiff_t sz, rtp_packet_t *out) {
    assert(out != NULL);
    assert(buf != NULL);
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
    assert(buf != NULL);
    assert(sequence_number_out != NULL);
    assert(ssrc_out != NULL);

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

void rtp_packet_print(const rtp_packet_t p) {
    ESP_LOGI(TAG, "RTP[v=%hhu ext=%hhu csrc=%hhu mark=%hhu pt=%hhu seq=%hu ts=%u ssrc=%u]",
             p.version, p.extension, p.csrc_count, p.marker, p.payload_type, p.sequence_number,
             p.timestamp, p.ssrc);
}

void init_rtp_jitbuf(const uint32_t ssrc, rtp_jitbuf_t *out) {
    assert(out != NULL);
    memset(out, 0, sizeof(*out));

    out->ssrc = ssrc;

    out->buf_top = -1;
    out->max_seq_out = -1;
}

static int mod(int a, int b) { return (a % b + b) % b; }

// TODO: Add seq num wrap-around handling.
// TODO: Add ts wrap-around handling.
esp_err_t rtp_jitbuf_feed(rtp_jitbuf_t *j, const uint8_t *buf, const ptrdiff_t sz) {
    uint16_t sequence_number = 0;
    uint32_t ssrc = 0;
    assert(sz > 0);
    esp_err_t err = partial_parse_rtp_packet(buf, sz, &sequence_number, &ssrc);
    if (err != ESP_OK) {
        return err;
    }

    if (ssrc != j->ssrc) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "->jitbuf state max_seq=%hu max_ts=%u buf_top=%d", j->max_seq, j->max_ts,
             j->buf_top);
    ESP_LOGD(TAG, "->jitbuf new packet seq=%hu ts=%u", sequence_number, timestamp);

    // Buffer is empty -> place at start.
    if (j->buf_top < 0) {
        ESP_LOGD(TAG, "->jitbuf empty, place at start");
        assert(j->max_seq == 0);
        j->max_seq = sequence_number;
        j->buf_top = 0;
        memcpy(j->buf[j->buf_top], buf, sz);
        j->buf_szs[j->buf_top] = sz;
        return ESP_OK;
    }

    // Figure out where to place the new packet relative to the current newest one.
    const int32_t advance = (int32_t)sequence_number - (int32_t)j->max_seq;
    ESP_LOGD(TAG, "->jitbuf advance %d", advance);

    if (advance == 0) {
        // Duplicate, drop.
        return ESP_OK;
    }

    if (advance > 0) {
        // TODO: no need to circle around more than RTP_JITBUF_BUF_N_PACKETS times..
        for (int32_t i = 0; i < advance; i++) {
            j->buf_top = (j->buf_top + 1) % RTP_JITBUF_BUF_N_PACKETS;
            if (j->buf_szs[j->buf_top] > 0) {
                ESP_LOGI(TAG, "->jitbuf dropping packet from end of buffer at %d", j->buf_top);
                memset(j->buf[j->buf_top], 0, j->buf_szs[j->buf_top]);
                j->buf_szs[j->buf_top] = 0;
            }
        }

        ESP_LOGD(TAG, "->jitbuf place packet at %d", j->buf_top);
        assert(sequence_number > j->max_seq);
        j->max_seq = sequence_number;
        memcpy(j->buf[j->buf_top], buf, sz);
        assert(j->buf_szs[j->buf_top] == 0);
        j->buf_szs[j->buf_top] = sz;

        return ESP_OK;
    }

    if (advance <= -RTP_JITBUF_BUF_N_PACKETS) {
        // Too old, drop.
        ESP_LOGI(TAG, "->jitbuf dropping incoming packet which is too late seq=%hu diff=%d",
                 sequence_number, advance);
        return ESP_OK;
    }

    // Place the packet somewhere in the middle.
    const int pos = mod(j->buf_top + advance, RTP_JITBUF_BUF_N_PACKETS);
    ESP_LOGD(TAG, "->jitbuf older packet seq=%hu diff=%d placing at %d", sequence_number, advance,
             pos);
    memcpy(j->buf[pos], buf, sz);
    assert(j->buf_szs[pos] == 0);  // TODO: support dropping of duplicate older packets
    j->buf_szs[pos] = sz;

    return ESP_OK;
}

static int rtp_jitbuf_find_oldest_packet(rtp_jitbuf_t *j) {
    if (j->buf_top < 0) {
        return -1;
    }

    // We start searching one after buf_top.
    for (int i = 1; i <= RTP_JITBUF_BUF_N_PACKETS; i++) {
        const int pos = (j->buf_top + i) % RTP_JITBUF_BUF_N_PACKETS;
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
    assert(sz >= RTP_JITBUF_PACKET_MAX_SIZE);
    assert(pos >= 0 && pos < RTP_JITBUF_BUF_N_PACKETS);
    ESP_LOGD(TAG, "jitbuf-> hand out buffer %d", pos);
    j->max_seq_out = sequence_number;

    // Copy out.
    const ptrdiff_t ret = j->buf_szs[pos];
    memcpy(buf, j->buf[pos], j->buf_szs[pos]);

    // Delete buf slot.
    memset(j->buf[pos], 0, j->buf_szs[pos]);
    j->buf_szs[pos] = 0;

    if (sequence_number == j->max_seq) {
        // We just handed out the last buffer.

        ESP_LOGD(TAG, "jitbuf-> is now empty, cleanup");

        j->max_seq = 0;
        j->buf_top = -1;

        for (int i = 0; i < RTP_JITBUF_BUF_N_PACKETS; i++) {
            assert(j->buf_szs[i] == 0);
        }
    }

    return ret;
}

static void rtp_jitbuf_print(rtp_jitbuf_t *j, const bool silence) {
    if (silence) {
        return;
    }

    for (int i = 0; i < RTP_JITBUF_BUF_N_PACKETS; i++) {
        if (i == j->buf_top) {
            printf("  |   ");
        } else {
            printf("      ");
        }
    }
    printf("\n");

    for (int i = 0; i < RTP_JITBUF_BUF_N_PACKETS; i++) {
        const ptrdiff_t sz = j->buf_szs[i];
        if (sz == 0) {
            printf("_____ ");
            continue;
        }

        uint16_t sequence_number = 0;
        uint32_t ssrc = 0;
        const esp_err_t err = partial_parse_rtp_packet(j->buf[i], sz, &sequence_number, &ssrc);
        assert(err == ESP_OK);

        printf("%5hu ", sequence_number);
    }

    printf("\n");
}

ptrdiff_t rtp_jitbuf_retrieve(rtp_jitbuf_t *j, uint8_t *buf, const ptrdiff_t sz) {
    ESP_LOGD(TAG, "jitbuf-> state max_seq=%hu max_ts=%u buf_top=%d max_seq_out=%d max_ts_out=%ld",
             j->max_seq, j->max_ts, j->buf_top, j->max_seq_out, j->max_ts_out);
    rtp_jitbuf_print(j, true);

    const int pos = rtp_jitbuf_find_oldest_packet(j);
    if (pos < 0) {
        ESP_LOGD(TAG, "jitbuf-> is empty");
        return 0;
    }

    // Parse the candidate.
    uint16_t sequence_number = 0;
    uint32_t ssrc = 0;
    const esp_err_t err =
        partial_parse_rtp_packet(j->buf[pos], j->buf_szs[pos], &sequence_number, &ssrc);
    assert(err == ESP_OK);

    ESP_LOGD(TAG, "jitbuf-> consider packet at %d seq=%hu ts=%u", pos, sequence_number, timestamp);

    if (j->max_seq_out < 0 || sequence_number == j->max_seq_out + 1) {
        ESP_LOGD(TAG, "jitbuf-> hand out packet next in seq");
        return rtp_jitbuf_hand_out_buffer(j, pos, sequence_number, buf, sz);
    }

    // Buffer is full, hand out the last packet.
    if (mod(pos - 1, RTP_JITBUF_BUF_N_PACKETS) == j->buf_top) {
        ESP_LOGD(TAG, "jitbuf-> hand out packet because buffer is full pos=%d top=%d", pos,
                 j->buf_top);
        return rtp_jitbuf_hand_out_buffer(j, pos, sequence_number, buf, sz);
    }

    ESP_LOGD(TAG, "jitbuf-> nothing to hand out pos=%d top=%d +1=", pos, j->buf_top);
    return 0;
}
