#include "rtp.h"

#include <arpa/inet.h>
#include <assert.h>
#include <string.h>

#include "fakesp.h"

static const char *TAG = "rtp";

ptrdiff_t parse_rtp_header(const uint8_t *data, ptrdiff_t length, rtp_header_t *header) {
    if (length < 12) {
        return 0;
    }

    memset(header, 0, sizeof(rtp_header_t));
    header->version = (data[0] >> 6) & 0x03;
    if (header->version != 2) {
        return 0;
    }

    header->padding = (data[0] >> 5) & 0x01;
    header->extension = (data[0] >> 4) & 0x01;
    header->csrc_count = data[0] & 0x0F;
    header->marker = (data[1] >> 7) & 0x01;
    header->payload_type = data[1] & 0x7F;
    header->sequence_number = (data[2] << 8) | data[3];
    header->timestamp = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    header->ssrc = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];

    assert(header->csrc_count <= 16);
    if (length < 12 + header->csrc_count * 4) {
        return 0;
    }
    for (uint8_t i = 0; i < header->csrc_count; i++) {
        header->csrc[i] = (data[12 + i * 4] << 24) | (data[13 + i * 4] << 16) |
                          (data[14 + i * 4] << 8) | data[15 + i * 4];
    }

    return 12 + header->csrc_count * 4;
}

void rtp_header_print(const rtp_header_t *h) {
    ESP_LOGI(TAG, "RTP[v=%hhu e=%hhu c=%hhu mark=%hhu pt=%hhu seq=%hu ts=%u ssrc=%u]\n", h->version,
             h->extension, h->csrc_count, h->marker, h->payload_type, h->sequence_number,
             h->timestamp, h->ssrc);
}
