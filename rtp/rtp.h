#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fakesp.h"

// https://datatracker.ietf.org/doc/html/rfc3550#section-5.1
//
// 0                   1                   2                   3
// 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |V=2|P|X|  CC   |M|     PT      |       sequence number         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                           timestamp                           |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |           synchronization source (SSRC) identifier            |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |            contributing source (CSRC) identifiers             |
// |                             ....                              |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct rtp_header_t {
    uint8_t version;
    uint8_t padding;
    uint8_t extension;
    uint8_t csrc_count;
    uint8_t marker;
    uint8_t payload_type;
    uint16_t sequence_number;
    uint32_t timestamp;
    uint32_t ssrc;
    uint32_t csrc[16];

    uint8_t *payload;
    ptrdiff_t payload_sz;
} rtp_header_t;

esp_err_t parse_rtp_header(const uint8_t *buf, const ptrdiff_t sz, rtp_header_t *out);

esp_err_t partial_parse_rtp_header(const uint8_t *buf, const ptrdiff_t sz,
                                   uint16_t *sequence_number_out, uint32_t *ssrc_out);

void rtp_header_print(const rtp_header_t h);

// https://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml
typedef enum rtp_pt {
    RTP_PT_JPEG = 26,
} rtp_pt;

// https://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml
typedef enum rtp_pt_clockrate {
    RTP_PT_CLOCKRATE_JPEG = 90000,
} rtp_pt_clockrate;
