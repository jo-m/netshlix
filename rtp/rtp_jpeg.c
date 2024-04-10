#include "rtp_jpeg.h"

#include <arpa/inet.h>
#include <assert.h>
#include <string.h>

#include "fakesp.h"

static const char *TAG = "mjpg";

ptrdiff_t parse_rtp_jpeg_header(const uint8_t *data, const ptrdiff_t length,
                                rtp_jpeg_header_t *header) {
    if (length < 8) {
        return 0;
    }

    memset(header, 0, sizeof(rtp_jpeg_header_t));
    header->type_specific = data[0];
    header->fragment_offset = (data[1] << 16) | (data[2] << 8) | data[3];
    header->type = data[4];
    header->q = data[5];
    header->width = data[6] * 8;
    if (header->width > 2040) {
        return 0;
    }
    header->height = data[7] * 8;
    if (header->height > 2040) {
        return 0;
    }

    return 8;
}

void rtp_jpeg_header_print(const rtp_jpeg_header_t h) {
    ESP_LOGI(TAG, "MJPG[typs=%hhu fof=%u t=%hhu q=%hhu sz=%hux%hu]", h.type_specific,
             h.fragment_offset, h.type, h.q, h.width, h.height);
}

esp_err_t create_rtp_jpeg_session(uint32_t ssrc, rtp_jpeg_session_t *out) {
    assert(out != NULL);
    memset(out, 0, sizeof(rtp_jpeg_session_t));
    out->ssrc = ssrc;

    return ESP_OK;
}

// Try to find out if a packet can be assigned to a frame.
// Returns the index of the frame if found.
static int frames_try_assign_packet(const rtp_jpeg_session_t *s, const rtp_header_t h,
                                    const rtp_jpeg_header_t jh) {
    for (int i = 0; i < RTP_JPEG_N_FRAMES_BUFFERED; i++) {
        const rtp_jpeg_frame_t *f = &s->frames[i];

        if (!f->in_use) {
            continue;
        }

        // The same timestamp MUST appear in each fragment of a given frame.
        if (f->rtp_timestamp != h.timestamp || f->height != jh.height || f->width != jh.width) {
            continue;
        }

        if (h.sequence_number > f->rtp_seq_first + RTP_JPEG_MAX_PACKETS_PER_FRAME) {
            continue;
        }

        return i;
    }

    return -1;
}

// Store a packet in a frame.
// TODO: deal with sequence number wraparound.
static void frame_write_packet(rtp_jpeg_frame_t *f, const rtp_header_t h,
                               const rtp_jpeg_header_t jh, const uint8_t *payload,
                               const ptrdiff_t sz) {
    // Initialize?
    if (!f->in_use) {
        ESP_LOGI(TAG, "Initialize new frame ts=%u", h.timestamp);
        f->in_use = true;
        f->rtp_timestamp = h.timestamp;
        f->rtp_seq_first = h.sequence_number;
        f->rtp_seq_last = h.sequence_number;

        f->width = jh.width;
        f->height = jh.height;

        memset(f->rtp_seq_mask, 0, sizeof(f->rtp_seq_mask));
        memset(f->payload, 0, sizeof(f->payload));
        f->payload_max_sz = 0;
    }

    //  The RTP marker bit MUST be set in the last packet of a frame.
    if (h.marker) {
        ESP_LOGI(TAG, "Last packet [%hu, %hu]", f->rtp_seq_first, h.sequence_number);
        f->rtp_seq_last = h.sequence_number;
    }

    // Set bitfield.
    const int seq_d = h.sequence_number - f->rtp_seq_first;
    int byte_ix = seq_d / 8;
    int bit_ix = seq_d % 8;
    f->rtp_seq_mask[byte_ix] |= 1 << bit_ix;

    // Write payload.
    assert(jh.fragment_offset + sz <= RTP_JPEG_MAX_PAYLOAD_SIZE_BYTES);
    memcpy(f->payload + jh.fragment_offset, payload, sz);

    // The fragment offset plus the length of the payload data in the packet MUST NOT exceed 2^24
    // bytes.
    assert(jh.fragment_offset + sz <= 16777216);
    if (jh.fragment_offset + sz > f->payload_max_sz) {
        f->payload_max_sz = sz + jh.fragment_offset;
    }
}

// Find the next free frame.
// If none is free, take the one with the oldest RTP timestamp.
// TODO: handle ts wraparound.
static int frames_get_next_free(rtp_jpeg_session_t *s) {
    uint32_t min_ts = 0xffffffff;
    int min_ts_ix = -1;

    for (int i = 0; i < RTP_JPEG_N_FRAMES_BUFFERED; i++) {
        rtp_jpeg_frame_t *f = &s->frames[i];

        if (!f->in_use) {
            ESP_LOGI(TAG, "Found free frame %d", i);
            return i;
        }

        if (f->rtp_timestamp < min_ts) {
            min_ts = f->rtp_timestamp;
            min_ts_ix = i;
        }
    }

    ESP_LOGI(TAG, "Dropping frame %d ts=%u", min_ts_ix, s->frames[min_ts_ix].rtp_timestamp);
    s->frames[min_ts_ix].in_use = 0;
    return min_ts_ix;
}

// TODO: handle wraparound.
static bool frame_is_complete(const rtp_jpeg_frame_t f) {
    assert(f.in_use);

    if (f.rtp_seq_first == f.rtp_seq_last) {
        return false;
    }

    const uint16_t expected = f.rtp_seq_last - f.rtp_seq_first + 1;

    int count = 0;
    for (size_t i = 0; i < sizeof(f.rtp_seq_mask); i++) {
        uint8_t b = f.rtp_seq_mask[i];
        for (int j = 0; j < 8; j++) {
            count += b & 1;
            b >>= 1;
        }
    }

    return count == expected;
}

esp_err_t rtp_jpeg_session_feed(rtp_jpeg_session_t *s, const uint8_t *data, const ptrdiff_t sz,
                                const rtp_mono_timestamp_us ts __attribute__((unused))) {
    // Parse RTP feader.
    rtp_header_t h = {0};
    const ptrdiff_t offset0 = parse_rtp_header(data, sz, &h);

    if (offset0 == 0) {
        return ESP_FAIL;
    }
    if (h.padding || h.extension || h.csrc_count || h.payload_type != RTP_PT_JPEG) {
        // We cannot handle that.
        return ESP_FAIL;
    }

    // Parse RTP JPEG header.
    rtp_jpeg_header_t jh = {0};
    const ptrdiff_t offset1 = parse_rtp_jpeg_header(data + offset0, sz - offset0, &jh);
    if (offset1 == 0) {
        return ESP_FAIL;
    }
    if (h.ssrc != s->ssrc) {
        // Not our session.
        return ESP_FAIL;
    }

    // Print.
    rtp_header_print(h);
    rtp_jpeg_header_print(jh);

    // Write data to frames.
    int frame_ix = frames_try_assign_packet(s, h, jh);
    if (frame_ix >= 0) {
        ESP_LOGI(TAG, "Write packet to existing frame %d", frame_ix);
    } else {
        frame_ix = frames_get_next_free(s);
        ESP_LOGI(TAG, "Write packet to new frame %d", frame_ix);
    }
    rtp_jpeg_frame_t *f = &s->frames[frame_ix];
    frame_write_packet(f, h, jh, data + offset0 + offset1, sz - offset0 - offset1);

    // Emit payload and free.
    if (frame_is_complete(*f)) {
        ESP_LOGI(TAG, "Received complete frame %d ts=%u", frame_ix, f->rtp_timestamp);
        s->frames[frame_ix].in_use = false;
    }

    return ESP_OK;
}
