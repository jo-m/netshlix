#pragma once

#include <stddef.h>
#include <stdint.h>

#include "fakesp.h"

/**
 * A parsed RTP packet as per
 * https://datatracker.ietf.org/doc/html/rfc3550#section-5.1.
 *
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |V=2|P|X|  CC   |M|     PT      |       sequence number         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           timestamp                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |           synchronization source (SSRC) identifier            |
 * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 * |            contributing source (CSRC) identifiers             |
 * |                             ....                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct rtp_packet_t {
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

    // Pointer to the payload, not owned by this struct.
    uint8_t *payload;
    ptrdiff_t payload_sz;
} rtp_packet_t;

/**
 * Parse a packet from a network buffer.
 * The buf and out params must not be NULL.
 * The payload pointer will point into buf.
 * Returns ESP_OK on success.
 */
esp_err_t parse_rtp_packet(const uint8_t *buf, const ptrdiff_t sz, rtp_packet_t *out);

/**
 * Parse only the sequence number and ssrc from a network buffer.
 * The buf and out params must not be NULL.
 * Returns ESP_OK on success.
 */
esp_err_t partial_parse_rtp_packet(const uint8_t *buf, const ptrdiff_t sz,
                                   uint16_t *sequence_number_out, uint32_t *ssrc_out);

// Print a packet via ESP_LOG().
void rtp_packet_print(const rtp_packet_t *h);

// https://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml
typedef enum rtp_pt {
    RTP_PT_JPEG = 26,
} rtp_pt;

// https://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml
typedef enum rtp_pt_clockrate {
    RTP_PT_CLOCKRATE_JPEG = 90000,
} rtp_pt_clockrate;

#ifndef ESP_PLATFORM
#define CONFIG_RTP_JITBUF_CAP_N_PACKETS (20)
#define CONFIG_RTP_JITBUF_CAP_PACKET_SIZE_BYTES (1400)
#endif

/**
 * A jitterbuffer reorders RTP packets and drops duplicates.
 * Will wait for missing packets until the buffer is full.
 * Packets arriving too late are dropped.
 * Use init_rtp_jitbuf() to initialize an instance before usage.
 * All struct members are private to the implementation.
 */
typedef struct rtp_jitbuf_t {
    uint32_t ssrc;

    uint16_t max_seq;  // Max seq number we currently have in the buffer.
    int buf_top;       // Wrap-around buf index pointing to the packet with the highest seq number.
                       // Negative if the buffer is empty.

    // Packet buffer. Spaced by sequence number, i.e. neighbors have a seq difference of 1.
    uint8_t buf[CONFIG_RTP_JITBUF_CAP_N_PACKETS][CONFIG_RTP_JITBUF_CAP_PACKET_SIZE_BYTES];
    // Keeps track of occupied slots in the buffer, and their sizes. Uses same indexing as buf.
    ptrdiff_t buf_szs[CONFIG_RTP_JITBUF_CAP_N_PACKETS];

    int32_t max_seq_out;
} rtp_jitbuf_t;

// Initialize a rtp_jitbuf_t instance.
void init_rtp_jitbuf(const uint32_t ssrc, rtp_jitbuf_t *j);

/**
 * Feed a packet to the jitter buffer.
 * Call this once per packet received from the network.
 * Packets with a different SSRC will be silently ignored.
 */
esp_err_t rtp_jitbuf_feed(rtp_jitbuf_t *j, const uint8_t *buf, const ptrdiff_t sz);

/**
 * Receive the next packet from the buffer.
 * Will write the data to buf (which has extent sz).
 * Fails if buf is too small to hold the output data.
 * Returns the number of bytes written to buf, or 0 if no packet was available.
 * After each call to rtp_jitbuf_feed(), this should be called repeatedly until no more packets are
 * available.
 */
ptrdiff_t rtp_jitbuf_retrieve(rtp_jitbuf_t *j, uint8_t *buf, const ptrdiff_t sz);
