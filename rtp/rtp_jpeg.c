#include "rtp_jpeg.h"

#include <arpa/inet.h>
#include <assert.h>
#include <string.h>

ptrdiff_t parse_rtp_jpeg_header(uint8_t *data, ptrdiff_t length, rtp_header_jpeg_t *header) {
    if (length < 8) {
        return 0;
    }

    memset(header, 0, sizeof(rtp_header_jpeg_t));
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
