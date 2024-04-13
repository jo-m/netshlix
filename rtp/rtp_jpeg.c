#include "rtp_jpeg.h"

#include <arpa/inet.h>
#include <assert.h>
#include <string.h>

#include "fakesp.h"

static const char *TAG = "mjpg";

esp_err_t parse_rtp_jpeg_packet(const uint8_t *buf, ptrdiff_t sz, rtp_jpeg_packet_t *out) {
    assert(out != NULL);
    assert(buf != NULL);
    memset(out, 0, sizeof(*out));

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

void rtp_jpeg_packet_print(const rtp_jpeg_packet_t p) {
    ESP_LOGI(TAG, "MJPG[typs=%hhu fof=%u t=%hhu q=%hhu sz=%hux%hu]", p.type_specific,
             p.fragment_offset, p.type, p.q, p.width, p.height);
}

esp_err_t parse_rtp_jpeg_qt_packet(const uint8_t *buf, ptrdiff_t sz, rtp_jpeg_qt_packet_t *out,
                                   ptrdiff_t *parsed_sz) {
    assert(buf != NULL);
    assert(out != NULL);
    assert(parsed_sz != NULL);
    memset(out, 0, sizeof(*out));
    *parsed_sz = 0;

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
    *parsed_sz = header_sz + out->length;

    return ESP_OK;
}

void rtp_jpeg_qt_packet_print(const rtp_jpeg_qt_packet_t p) {
    ESP_LOGI(TAG, "QT[mbz=%hhu prec=%hhu len=%u]", p.mbz, p.precision, p.length);
}

void init_rtp_jpeg_session(const uint32_t ssrc, rtp_jpeg_session_t *out) {
    assert(out != NULL);
    memset(out, 0, sizeof(*out));
    out->ssrc = ssrc;
}

static esp_err_t rtp_jpeg_handle_frame(rtp_jpeg_session_t *s) {
    if (s->payload_sz < 2) {
        return ESP_ERR_INVALID_STATE;
    }

    // TODO: emit frame
    ESP_LOGI(TAG, "=== EMIT FRAME ===");

    return ESP_OK;
}

esp_err_t rtp_jpeg_session_feed(rtp_jpeg_session_t *s, const rtp_packet_t p) {
    rtp_packet_print(p);

    if (p.padding || p.extension || p.csrc_count || p.payload_type != RTP_PT_JPEG) {
        // We cannot handle that.
        ESP_LOGI(TAG, "Not supported");
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (p.ssrc != s->ssrc) {
        // Not our session.
        ESP_LOGI(TAG, "Not our session");
        return ESP_ERR_INVALID_ARG;
    }

    // Parse RTP JPEG header.
    rtp_jpeg_packet_t jp = {0};
    const esp_err_t err = parse_rtp_jpeg_packet(p.payload, p.payload_sz, &jp);
    if (err != ESP_OK) {
        return err;
    }

    // TODO: eventually support jp.q < 128.
    if (!(jp.type == 1 && jp.type_specific == 0 && jp.q >= 128)) {
        // We cannot handle that.
        ESP_LOGI(TAG, "Not supported");
        return ESP_ERR_NOT_SUPPORTED;
    }

    rtp_jpeg_packet_print(jp);

    if (jp.fragment_offset == 0) {
        // New frame, reset.
        init_rtp_jpeg_session(s->ssrc, s);

        // Parse quantization table.
        rtp_jpeg_qt_packet_t qt = {0};
        ptrdiff_t qt_parsed_sz = 0;
        const esp_err_t err2 =
            parse_rtp_jpeg_qt_packet(jp.payload, jp.payload_sz, &qt, &qt_parsed_sz);
        if (err2 != ESP_OK) {
            return err2;
        }
        rtp_jpeg_qt_packet_print(qt);

        // Copy quantization table.
        assert(qt.length == sizeof(s->qt_data));
        memcpy(s->qt_data, qt.payload, qt.length);
        s->qt_packet = qt;
        s->qt_packet.payload = s->qt_data;

        // Copy fragment.
        const ptrdiff_t payload_sz = jp.payload_sz - qt_parsed_sz;
        assert(payload_sz >= 0);
        memcpy(s->payload, jp.payload + qt_parsed_sz, payload_sz);
        s->payload_sz = payload_sz;

        ESP_LOGI(TAG, "Added QT jp.payload_sz=%ld qt_parsed_sz=%ld s->payload_sz=%ld",
                 jp.payload_sz, qt_parsed_sz, s->payload_sz);
    } else {
        if (jp.fragment_offset != s->payload_sz) {
            s->payload_sz = 0;
            return ESP_ERR_INVALID_STATE;
        }

        if (jp.payload_sz + s->payload_sz > (ptrdiff_t)sizeof(s->payload)) {
            s->payload_sz = 0;
            return ESP_ERR_NO_MEM;
        }

        memcpy(&s->payload[s->payload_sz], jp.payload, jp.payload_sz);
        s->payload_sz += jp.payload_sz;
    }

    if (p.marker == 0) {
        return ESP_OK;
    }

    const esp_err_t success = rtp_jpeg_handle_frame(s);
    s->payload_sz = 0;

    return success;
}
