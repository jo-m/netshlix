#include "rtp_jpeg.h"

#include <arpa/inet.h>
#include <assert.h>
#include <string.h>

#include "fakesp.h"

static const char *TAG = "mjpg";

esp_err_t parse_rtp_jpeg_header(const uint8_t *buf, ptrdiff_t sz, rtp_jpeg_header_t *out) {
    assert(out != NULL);
    assert(buf != NULL);
    memset(out, 0, sizeof(rtp_jpeg_header_t));

    const ptrdiff_t header_sz = 8;
    if (sz < header_sz) {
        return ESP_ERR_INVALID_SIZE;
    }

    out->type_specific = buf[0];
    out->fragment_offset = (buf[1] << 16) | (buf[2] << 8) | buf[3];
    out->type = buf[4];
    out->q = buf[5];
    out->width = buf[6] * 8;
    if (out->width > 2040) {
        return ESP_ERR_INVALID_ARG;
    }
    out->height = buf[7] * 8;
    if (out->height > 2040) {
        return ESP_ERR_INVALID_ARG;
    }

    out->payload = (uint8_t *)&buf[header_sz];
    out->payload_sz = sz - header_sz;
    return ESP_OK;
}

void rtp_jpeg_header_print(const rtp_jpeg_header_t h) {
    ESP_LOGI(TAG, "MJPG[typs=%hhu fof=%u t=%hhu q=%hhu sz=%hux%hu]", h.type_specific,
             h.fragment_offset, h.type, h.q, h.width, h.height);
}

esp_err_t parse_rtp_jpeg_qt_header(const uint8_t *buf, ptrdiff_t sz, rtp_jpeg_qt_header_t *out) {
    assert(out != NULL);
    assert(buf != NULL);
    memset(out, 0, sizeof(rtp_jpeg_qt_header_t));

    const ptrdiff_t header_sz = 4;
    if (sz < header_sz) {
        return ESP_ERR_INVALID_SIZE;
    }

    out->mbz = buf[0];
    out->precision = buf[1];
    out->length = (buf[2] << 8) | buf[3];

    if (header_sz + out->length > sz) {
        return ESP_ERR_INVALID_SIZE;
    }

    out->payload = (uint8_t *)&buf[header_sz];
    out->payload_sz = out->length;

    return ESP_OK;
}

void rtp_jpeg_qt_header_print(const rtp_jpeg_qt_header_t h) {
    ESP_LOGI(TAG, "QT[mbz=%hhu prec=%hhu len=%u]", h.mbz, h.precision, h.length);
}

void init_rtp_jpeg_session(const uint32_t ssrc, rtp_jpeg_session_t *out) {
    assert(out != NULL);
    memset(out, 0, sizeof(rtp_jpeg_session_t));
    out->ssrc = ssrc;
}

// Try to find out if a packet can be assigned to a frame.
// Returns the index of the frame if found.
static int frames_try_assign_packet(const rtp_jpeg_session_t s, const rtp_header_t h,
                                    const rtp_jpeg_header_t jh) {
    for (int i = 0; i < RTP_JPEG_N_FRAMES_BUFFERED; i++) {
        const rtp_jpeg_frame_t *f = &s.frames[i];

        if (!f->in_use) {
            continue;
        }

        // The same timestamp MUST appear in each fragment of a given frame.
        if (h.timestamp != f->rtp_timestamp) {
            continue;
        }

        if (h.sequence_number > f->rtp_seq_first + RTP_JPEG_MAX_PACKETS_PER_FRAME) {
            return -2;
        }

        //  All fields in this header except for the Fragment Offset field MUST remain the same in
        //  all packets that correspond to the same JPEG frame.
        if (jh.type_specific != f->example.type_specific || jh.type != f->example.type ||
            jh.q != f->example.q || jh.width != f->example.width ||
            jh.height != f->example.height) {
            return -3;
        }

        return i;
    }

    return -1;
}

// Store a packet in a frame.
// TODO: deal with sequence number wraparound.
static esp_err_t frame_write_packet(rtp_jpeg_frame_t *f, const rtp_header_t h,
                                    const rtp_jpeg_header_t jh) {
    // Initialize?
    if (!f->in_use) {
        ESP_LOGI(TAG, "Initialize new frame ts=%u", h.timestamp);
        f->in_use = true;
        f->rtp_timestamp = h.timestamp;
        f->rtp_seq_first = h.sequence_number;
        f->rtp_seq_last = h.sequence_number;

        f->example = jh;
        f->example.fragment_offset = 0;
        f->example.payload = NULL;

        // TODO: this should already have been done on cleanup.
        memset(f->rtp_seq_mask, 0, sizeof(f->rtp_seq_mask));
        memset(f->payload, 0, sizeof(f->payload));
        f->payload_sz = 0;
    }

    // Quantization table.
    if (jh.q >= 128 && jh.fragment_offset == 0) {
        rtp_jpeg_qt_header_t qt = {0};
        const esp_err_t success = parse_rtp_jpeg_qt_header(jh.payload, jh.payload_sz, &qt);
        if (success != ESP_OK) {
            return success;
        }
        rtp_jpeg_qt_header_print(qt);

        // TODO: advance payload?
    }

    //  Mark frame as potentially complete on marker bit.
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
    const ptrdiff_t max_sz = jh.fragment_offset + jh.payload_sz;
    assert(max_sz <= RTP_JPEG_MAX_PAYLOAD_SIZE_BYTES);
    assert(max_sz <= (1 << 24));  // According to RFC 2435.
    memcpy(f->payload + jh.fragment_offset, jh.payload, jh.payload_sz);

    if (max_sz > f->payload_sz) {
        f->payload_sz = max_sz;
    }

    return ESP_OK;
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

    ESP_LOGI(TAG, "Dropping incomplete frame %d ts=%u", min_ts_ix,
             s->frames[min_ts_ix].rtp_timestamp);
    memset(&s->frames[min_ts_ix], 0, sizeof(rtp_jpeg_frame_t));
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

esp_err_t rtp_jpeg_session_feed(rtp_jpeg_session_t *s, const rtp_header_t h) {
    if (h.padding || h.extension || h.csrc_count || h.payload_type != RTP_PT_JPEG) {
        // We cannot handle that.
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (h.ssrc != s->ssrc) {
        // Not our session.
        return ESP_ERR_INVALID_ARG;
    }

    // Parse RTP JPEG header.
    rtp_jpeg_header_t jh = {0};
    const esp_err_t success = parse_rtp_jpeg_header(h.payload, h.payload_sz, &jh);
    if (success != ESP_OK) {
        return success;
    }

    // Print.
    rtp_header_print(h);
    rtp_jpeg_header_print(jh);

    // TODO: handle this properly.
    assert(jh.type == 1 && jh.type_specific == 0 && jh.q >= 128 && jh.q <= 255);

    // Write data to frames.
    int frame_ix = frames_try_assign_packet(*s, h, jh);
    if (frame_ix >= 0) {
        ESP_LOGI(TAG, "Write packet to existing frame %d", frame_ix);
    } else {
        frame_ix = frames_get_next_free(s);
        ESP_LOGI(TAG, "Write packet to new frame %d", frame_ix);
    }
    rtp_jpeg_frame_t *f = &s->frames[frame_ix];
    frame_write_packet(f, h, jh);

    // Emit payload and free.
    if (frame_is_complete(*f)) {
        ESP_LOGI(TAG, "Received complete frame %d ts=%u", frame_ix, f->rtp_timestamp);
        s->frames[frame_ix].in_use = false;
    }

    return ESP_OK;
}
