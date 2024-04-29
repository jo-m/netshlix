#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <freertos/FreeRTOS.h>
#pragma GCC diagnostic pop
#include <stddef.h>

#include "rtp_jpeg.h"
#include "sdkconfig.h"

#if !CONFIG_LWIP_NETBUF_RECVINFO
#error "Needs CONFIG_LWIP_NETBUF_RECVINFO=y to work!"
#endif

size_t rtp_udp_recv_task_approx_stack_sz();

// Task to receive UDP/RTP packets and depayload them into JPEG frames.
// Expects a QueueHandle_t<uint8_t[CONFIG_RTP_JPEG_MAX_DATA_SIZE_BYTES]> as pvParameters argument.
void rtp_udp_recv_task(void *pvParameters);
