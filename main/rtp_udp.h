#pragma once

#include <sdkconfig.h>

#if !CONFIG_LWIP_NETBUF_RECVINFO
#error "Needs CONFIG_LWIP_NETBUF_RECVINFO=y to work!"
#endif

// Task to receive UDP/RTP packets and depayload them into JPEG frames.
// Expects a QueueHandle_t as only argument.
void rtp_udp_recv_task(void *pvParameters);
