#include "rtp_jpeg.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#include "fakesp.h"
#include "rfc2435.h"

__attribute__((unused)) static const char *TAG = "mjpg";

esp_err_t parse_rtp_jpeg_packet(const uint8_t *buf, ptrdiff_t sz, rtp_jpeg_packet_t *out) {
    if (out == NULL || buf == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));

    const ptrdiff_t header_sz = 8;
    if (sz < header_sz) {
        return ESP_ERR_INVALID_SIZE;
    }

    out->type_specific = buf[0];
    out->fragment_offset = (buf[1] << 16) | (buf[2] << 8) | buf[3];
    out->type = buf[4];
    out->q = buf[5];
    if (out->q == 0) {
        return ESP_ERR_INVALID_ARG;
    }
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

void rtp_jpeg_packet_print(const rtp_jpeg_packet_t *p __attribute__((unused))) {
    assert(p != NULL);
    ESP_LOGD(TAG,
             "RTP/JPEG[typs=%" PRIu8 " fof=%" PRIu32 " t=%" PRIu8 " q=%" PRIu8 " sz=%" PRIu16
             "x%" PRIu16 "]",
             p->type_specific, p->fragment_offset, p->type, p->q, p->width, p->height);
}

esp_err_t parse_rtp_jpeg_qt(const uint8_t *buf, ptrdiff_t sz, rtp_jpeg_qt_t *out,
                            ptrdiff_t *parsed_sz) {
    if (out == NULL || buf == NULL || parsed_sz == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    *parsed_sz = 0;

    const ptrdiff_t header_sz = 4;
    if (sz < header_sz) {
        return ESP_ERR_INVALID_SIZE;
    }

    out->mbz = buf[0];
    out->precision = buf[1];
    out->length = (buf[2] << 8) | buf[3];
    if (out->length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (header_sz + out->length > sz) {
        return ESP_ERR_INVALID_SIZE;
    }

    out->payload = (uint8_t *)&buf[header_sz];
    out->payload_sz = out->length;
    *parsed_sz = header_sz + out->length;

    return ESP_OK;
}

void rtp_jpeg_qt_print(const rtp_jpeg_qt_t *p __attribute__((unused))) {
    assert(p != NULL);
    ESP_LOGD(TAG, "QT[mbz=%hhu prec=%hhu len=%u]", p->mbz, p->precision, p->length);
}

void init_rtp_jpeg_session(const uint32_t ssrc, rtp_jpeg_frame_cb frame_cb, void *userdata,
                           rtp_jpeg_session_t *s) {
    assert(frame_cb != NULL);
    assert(s != NULL);
    memset(s, 0, sizeof(*s));
    s->ssrc = ssrc;
    s->frame_cb = frame_cb;
    s->userdata = userdata;
}

static esp_err_t rtp_jpeg_write_header(rtp_jpeg_session_t *s, const rtp_jpeg_qt_t *qt) {
    assert(qt != NULL);
    // We only support 8 bit precision Q tables for now.
    if (qt->payload_sz != 128 || (qt->precision & 1) || (qt->precision & 2)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    const ptrdiff_t lqt_sz = (qt->precision & 1) ? 128 : 64;

    _Static_assert(sizeof(s->jpeg_data) >= RFC2435_HEADER_MAX_SIZE_BYTES, "Buffer too small");
    const ptrdiff_t jfif_header_sz =
        rfc2435_make_headers(&s->jpeg_data[0], s->header.type, s->header.width >> 3,
                             s->header.height >> 3, &qt->payload[0], &qt->payload[lqt_sz], 0);
    assert(jfif_header_sz <= RFC2435_HEADER_MAX_SIZE_BYTES);

    s->jpeg_data_sz = jfif_header_sz;
    s->jfif_header_sz = jfif_header_sz;

    return ESP_OK;
}

static esp_err_t rtp_jpeg_handle_frame(const rtp_jpeg_session_t *s) {
    if (s->jfif_header_sz == 0 || s->jpeg_data_sz < s->jfif_header_sz + 2) {
        return ESP_ERR_INVALID_STATE;
    }

    // Emit frame callback.
    rtp_jpeg_frame_t frame = {0};
    frame.width = s->header.width;
    frame.height = s->header.height;
    frame.timestamp = s->rtp_timestamp;
    frame.jpeg_data = s->jpeg_data;
    frame.jpeg_data_sz = s->jpeg_data_sz;
    frame.jfif_header_sz = s->jfif_header_sz;

    assert(s->frame_cb != NULL);
    s->frame_cb(&frame, s->userdata);

    return ESP_OK;
}

esp_err_t rtp_jpeg_session_feed(rtp_jpeg_session_t *s, const rtp_packet_t *p) {
    assert(s != NULL);
    if (p == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    rtp_packet_print(p);

    if (p->padding || p->extension || p->csrc_count || p->payload_type != RTP_PT_JPEG) {
        // We cannot handle that.
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (p->ssrc != s->ssrc) {
        // Not our session.
        return ESP_ERR_INVALID_ARG;
    }

    // Parse RTP JPEG header.
    rtp_jpeg_packet_t jp = {0};
    const esp_err_t err = parse_rtp_jpeg_packet(p->payload, p->payload_sz, &jp);
    if (err != ESP_OK) {
        return err;
    }

    // TODO: eventually support jp.q < 128.
    if (!(jp.type == 1 && jp.type_specific == 0 && jp.q >= 128)) {
        // We cannot handle that.
        return ESP_ERR_NOT_SUPPORTED;
    }

    rtp_jpeg_packet_print(&jp);

    if (jp.fragment_offset == 0) {
        // New frame, reset.
        init_rtp_jpeg_session(s->ssrc, s->frame_cb, s->userdata, s);

        // Copy header.
        s->header = jp;
        s->header.payload = NULL;
        s->header.payload_sz = 0;
        s->rtp_timestamp = p->timestamp;

        // Parse quantization table.
        rtp_jpeg_qt_t qt = {0};
        ptrdiff_t qt_parsed_sz = 0;
        const esp_err_t err2 = parse_rtp_jpeg_qt(jp.payload, jp.payload_sz, &qt, &qt_parsed_sz);
        if (err2 != ESP_OK) {
            return err2;
        }
        rtp_jpeg_qt_print(&qt);

        // Write JFIF header to data buffer.
        const esp_err_t err3 = rtp_jpeg_write_header(s, &qt);
        if (err3 != ESP_OK) {
            return err3;
        }

        // Copy fragment.
        const ptrdiff_t payload_sz = jp.payload_sz - qt_parsed_sz;
        assert(payload_sz >= 0);
        memcpy(s->jpeg_data + s->jpeg_data_sz, jp.payload + qt_parsed_sz, payload_sz);
        s->jpeg_data_sz += payload_sz;

        ESP_LOGD(TAG, "Added QT jp.payload_sz=%ld qt_parsed_sz=%ld s->payload_sz=%ld",
                 (long)jp.payload_sz, (long)qt_parsed_sz, (long)s->jpeg_data_sz);
    } else {
        if (jp.type_specific != s->header.type_specific || jp.type != s->header.type ||
            // Does it match the first packet?
            jp.q != s->header.q || jp.width != s->header.width || jp.height != s->header.height) {
            s->jpeg_data_sz = 0;
            return ESP_ERR_INVALID_STATE;
        }

        if ((int64_t)jp.fragment_offset + s->jfif_header_sz != s->jpeg_data_sz) {
            s->jpeg_data_sz = 0;
            return ESP_ERR_INVALID_STATE;
        }

        if (s->jpeg_data_sz + jp.payload_sz > (ptrdiff_t)sizeof(s->jpeg_data)) {
            s->jpeg_data_sz = 0;
            return ESP_ERR_NO_MEM;
        }

        memcpy(&s->jpeg_data[s->jpeg_data_sz], jp.payload, jp.payload_sz);
        s->jpeg_data_sz += jp.payload_sz;
    }

    if (p->marker == 0) {
        return ESP_OK;
    }

    const esp_err_t success = rtp_jpeg_handle_frame(s);
    s->jpeg_data_sz = 0;

    return success;
}
