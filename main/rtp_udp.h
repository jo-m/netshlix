#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <freertos/FreeRTOS.h>
#pragma GCC diagnostic pop
#include <stddef.h>

#include "sdkconfig.h"

#if !CONFIG_LWIP_NETBUF_RECVINFO
#error "Needs CONFIG_LWIP_NETBUF_RECVINFO=y to work!"
#endif

typedef struct rtp_udp_outbuf_t {
    QueueHandle_t mut;
    uint8_t buf[CONFIG_RTP_JPEG_MAX_DATA_SIZE_BYTES];
    ptrdiff_t buf_sz;
} rtp_udp_outbuf_t;

void init_rtp_udp_outbuf(rtp_udp_outbuf_t *b);

size_t rtp_udp_recv_task_approx_stack_sz();

// Task to receive UDP/RTP packets and depayload them into JPEG frames.
// Expects a rtp_udp_outbuf_t as pvParameters argument.
void rtp_udp_recv_task(void *pvParameters);
