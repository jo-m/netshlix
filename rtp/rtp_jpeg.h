#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fakesp.h"
#include "rtp.h"

// A parsed RTP JPEG packet as per
// https://datatracker.ietf.org/doc/html/rfc2435#section-3.1.
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

    // Pointer to the payload, not owned by this struct.
    uint8_t *payload;
    ptrdiff_t payload_sz;
} rtp_jpeg_packet_t;

// Parse a packet from a network buffer.
// The buf and out params must not be NULL.
// The payload pointer will point into buf.
// Returns ESP_OK on success.
esp_err_t parse_rtp_jpeg_packet(const uint8_t *buf, ptrdiff_t sz, rtp_jpeg_packet_t *out);

// Print a packet via ESP_LOG().
void rtp_jpeg_packet_print(const rtp_jpeg_packet_t h);

// A parsed JPEG quantization table header+payload as per
// https://datatracker.ietf.org/doc/html/rfc2435#section-3.1.8.
//
// 0                   1                   2                   3
// 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |      MBZ      |   Precision   |             Length            |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                    Quantization Table Data                    |
// |                              ...                              |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct rtp_jpeg_qt_t {
    uint8_t mbz;
    uint8_t precision;
    uint16_t length;

    // Pointer to the payload, not owned by this struct.
    uint8_t *payload;
    ptrdiff_t payload_sz;
} rtp_jpeg_qt_t;

// Parse a quantization table from a network buffer.
// The buf and out params must not be NULL.
// The payload pointer will point into buf.
// *parsed_sz will be set to the number of bytes parsed.
// Returns ESP_OK on success.
esp_err_t parse_rtp_jpeg_qt(const uint8_t *buf, ptrdiff_t sz, rtp_jpeg_qt_t *out,
                            ptrdiff_t *parsed_sz);

// Print a quantization table header via ESP_LOG().
void rtp_jpeg_qt_print(const rtp_jpeg_qt_t h);

#define RTP_JPEG_MAX_FRAGMENTS_SIZE_BYTES (50 * 1024)
#define RTP_JPEG_QT_DATA_SIZE_BYTES 128

// A RTP/JPEG session de-payloads and assembles JPEG frames from RTP packets.
// Use init_rtp_jpeg_session() to initialize an instance before usage.
typedef struct rtp_jpeg_session_t {
    uint32_t ssrc;

    // Example header of the current frame being assembled.
    // Its payload will be set to NULL, we only care about the metadata.
    rtp_jpeg_packet_t header;

    // JPEG fragments payload.
    uint8_t fragments[RTP_JPEG_MAX_FRAGMENTS_SIZE_BYTES];
    ptrdiff_t fragments_sz;

    // Quantization table header, payload will point to qt_data.
    rtp_jpeg_qt_t qt_header;
    uint8_t qt_data[RTP_JPEG_QT_DATA_SIZE_BYTES];
} rtp_jpeg_session_t;

// Initialize a session with a given SSRC.
void init_rtp_jpeg_session(const uint32_t ssrc, rtp_jpeg_session_t *s);

// Feed a RTP packet to an RTP/JPEG session.
// Packets are expected to be ordered and deduplicated (use jitbuf for this).
esp_err_t rtp_jpeg_session_feed(rtp_jpeg_session_t *s, const rtp_packet_t h);
