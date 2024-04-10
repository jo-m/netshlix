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

esp_err_t rtp_jpeg_session_feed(rtp_jpeg_session_t *s, const uint8_t *data, const ptrdiff_t sz,
                                const rtp_mono_timestamp_us ts) {
    rtp_header_t h = {0};
    const ptrdiff_t offset0 = parse_rtp_header(data, sz, &h);
    if (offset0 == 0) {
        return ESP_FAIL;
    }

    if (h.padding || h.extension || h.csrc_count || h.payload_type != RTP_PT_JPEG) {
        return ESP_FAIL;
    }

    rtp_jpeg_header_t hj = {0};
    const ptrdiff_t offset1 = parse_rtp_jpeg_header(data + offset0, sz - offset0, &hj);
    if (offset1 == 0) {
        return ESP_FAIL;
    }

    if (h.ssrc != s->ssrc) {
        return ESP_FAIL;
    }

    rtp_header_print(&h);
    rtp_jpeg_header_print(&hj);

    s->last_recv = ts;
    return ESP_OK;
}
