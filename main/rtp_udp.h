#pragma once

#include <sdkconfig.h>
#include <stddef.h>

#if !CONFIG_LWIP_NETBUF_RECVINFO
#error "Needs CONFIG_LWIP_NETBUF_RECVINFO=y to work!"
#endif

size_t rtp_udp_recv_task_approx_stack_sz();

// Task to receive UDP/RTP packets and depayload them into JPEG frames.
// Expects a QueueHandle_t as pvParameters argument.
void rtp_udp_recv_task(void *pvParameters);
