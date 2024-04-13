#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fakesp.h"
#include "rtp.h"

#define RTP_JPEG_N_FRAMES_BUFFERED 5
#define RTP_JPEG_MAX_PACKETS_PER_FRAME 24
#define RTP_JPEG_MAX_FRAGMENTS_SIZE_BYTES (50 * 1024)
#define RTP_JPEG_QT_DATA_SIZE_BYTES 128

// https://datatracker.ietf.org/doc/html/rfc2435#section-3.1
//
// 0                   1                   2                   3
// 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// | Type-specific |              Fragment Offset                  |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |      Type     |       Q       |     Width     |     Height    |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct rtp_jpeg_packet_t {
    uint8_t type_specific;
    uint32_t fragment_offset;
    uint8_t type;
    uint8_t q;
    uint16_t width;
    uint16_t height;

    uint8_t *payload;
    ptrdiff_t payload_sz;
} rtp_jpeg_packet_t;

esp_err_t parse_rtp_jpeg_packet(const uint8_t *buf, ptrdiff_t sz, rtp_jpeg_packet_t *out);

void rtp_jpeg_packet_print(const rtp_jpeg_packet_t h);

// https://datatracker.ietf.org/doc/html/rfc2435#section-3.1.8
//
// 0                   1                   2                   3
// 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |      MBZ      |   Precision   |             Length            |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                    Quantization Table Data                    |
// |                              ...                              |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct rtp_jpeg_qt_packet_t {
    uint8_t mbz;
    uint8_t precision;
    uint16_t length;

    uint8_t *payload;
    ptrdiff_t payload_sz;
} rtp_jpeg_qt_packet_t;

esp_err_t parse_rtp_jpeg_qt_packet(const uint8_t *buf, ptrdiff_t sz, rtp_jpeg_qt_packet_t *out,
                                   ptrdiff_t *parsed_sz);

void rtp_jpeg_qt_packet_print(const rtp_jpeg_qt_packet_t h);

typedef struct rtp_jpeg_session_t {  // TODO: rename
    uint32_t ssrc;

    rtp_jpeg_packet_t header;  // Payload will be NULL.
    uint8_t fragments[RTP_JPEG_MAX_FRAGMENTS_SIZE_BYTES];
    ptrdiff_t fragments_sz;

    rtp_jpeg_qt_packet_t qt_header;
    uint8_t qt_data[RTP_JPEG_QT_DATA_SIZE_BYTES];
} rtp_jpeg_session_t;

void init_rtp_jpeg_session(const uint32_t ssrc, rtp_jpeg_session_t *out);

// Feed a packet to an RTP/JPEG session.
// Packets are expected to be ordered and deduplicated.
esp_err_t rtp_jpeg_session_feed(rtp_jpeg_session_t *s, const rtp_packet_t h);
