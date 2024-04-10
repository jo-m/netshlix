#include "rtp.h"

#include <arpa/inet.h>
#include <assert.h>
#include <string.h>

static const char *TAG = "rtp";
static const ptrdiff_t HEADER_MIN_SZ = 12;

esp_err_t parse_rtp_header(const uint8_t *buf, const ptrdiff_t sz, rtp_header_t *out) {
    assert(out != NULL);
    assert(buf != NULL);
    memset(out, 0, sizeof(rtp_header_t));

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

esp_err_t partial_parse_rtp_header(const uint8_t *buf, const ptrdiff_t sz,
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

void rtp_header_print(const rtp_header_t h) {
    ESP_LOGI(TAG, "RTP[v=%hhu e=%hhu c=%hhu mark=%hhu pt=%hhu seq=%hu ts=%u ssrc=%u]", h.version,
             h.extension, h.csrc_count, h.marker, h.payload_type, h.sequence_number, h.timestamp,
             h.ssrc);
}
