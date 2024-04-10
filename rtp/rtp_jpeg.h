#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fakesp.h"
#include "rtp.h"

#define RTP_JPEG_N_FRAMES_BUFFERED 3
#define RTP_JPEG_MAX_PACKETS_PER_FRAME 24
#define RTP_JPEG_MAX_PAYLOAD_SIZE_BYTES (50 * 1024)

// https://datatracker.ietf.org/doc/html/rfc2435#section-3.1
//
// 0                   1                   2                   3
// 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// | Type-specific |              Fragment Offset                  |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |      Type     |       Q       |     Width     |     Height    |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct rtp_jpeg_header_t {
    uint8_t type_specific;
    uint32_t fragment_offset;
    uint8_t type;
    uint8_t q;
    uint16_t width;
    uint16_t height;
} rtp_jpeg_header_t;

ptrdiff_t parse_rtp_jpeg_header(const uint8_t *data, ptrdiff_t length, rtp_jpeg_header_t *out);

void rtp_jpeg_header_print(const rtp_jpeg_header_t h);

typedef struct rtp_jpeg_frame_t {
    bool in_use;
    rtp_mono_timestamp_us first_pack_recv_timestamp;
    uint32_t rtp_timestamp;
    uint16_t width;
    uint16_t height;

    uint16_t first_rtp_sequence_number;
    uint8_t received_packets_bitfield[RTP_JPEG_MAX_PACKETS_PER_FRAME / 8 +
                                      (RTP_JPEG_MAX_PACKETS_PER_FRAME % 8 != 0)];
    uint8_t payload[RTP_JPEG_MAX_PAYLOAD_SIZE_BYTES];
} rtp_jpeg_frame_t;

typedef struct rtp_jpeg_session_t {
    uint32_t ssrc;

    rtp_mono_timestamp_us last_recv;
    rtp_jpeg_frame_t frames[RTP_JPEG_N_FRAMES_BUFFERED];
} rtp_jpeg_session_t;

esp_err_t create_rtp_jpeg_session(uint32_t ssrc, rtp_jpeg_session_t *out);

esp_err_t rtp_jpeg_session_feed(rtp_jpeg_session_t *s, const uint8_t *pack, const ptrdiff_t sz,
                                const rtp_mono_timestamp_us ts);
