#include "rtp_udp.h"

#include <string.h>
#include <sys/param.h>

#include "esp_err.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <freertos/FreeRTOS.h>
#pragma GCC diagnostic pop
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/task.h>
#include <lwip/err.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <nvs_flash.h>

#include "rtp.h"
#include "rtp_jpeg.h"

static const char *TAG = "rtp_udp";

_Static_assert(CONFIG_RTP_JITBUF_CAP_PACKET_SIZE_BYTES == CONFIG_SMALLTV_UDP_PAYLOAD_BYTES,
               "Jitterbuffer packet size should be equal to UDP MTU!");

typedef struct rtp_udp_t {
    QueueHandle_t out;

    int sock;
    struct sockaddr_storage source_addr;

    char rx_buf[CONFIG_SMALLTV_UDP_PAYLOAD_BYTES];
    ptrdiff_t rx_sz;
    struct iovec iov;
    struct msghdr msg;
} rtp_udp_t;

static esp_err_t sock_bind_prepare(rtp_udp_t *u) {
    assert(u != NULL);

    struct sockaddr_in dest_addr = {0};
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(CONFIG_SMALLTV_RTP_PORT);

    assert(u->sock == -1);
    u->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (u->sock < 0) {
        ESP_LOGE(TAG, "socket() failed: errno %d", errno);
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "Socket created");

    const int enable = 1;
    lwip_setsockopt(u->sock, IPPROTO_IP, IP_PKTINFO, &enable, sizeof(enable));

    // Set timeout.
    struct timeval timeout = {0};
    timeout.tv_sec = CONFIG_SMALLTV_UDP_RECV_TIMEOUT_S;
    timeout.tv_usec = 0;
    setsockopt(u->sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

    const int err = bind(u->sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "bind() failed: errno %d", errno);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", CONFIG_SMALLTV_RTP_PORT);

    // Prepare for receiving.
    u->iov.iov_base = u->rx_buf;
    u->iov.iov_len = sizeof(u->rx_buf);
    u->msg.msg_control = NULL;
    u->msg.msg_controllen = 0;
    u->msg.msg_flags = 0;
    u->msg.msg_iov = &u->iov;
    u->msg.msg_iovlen = 1;
    u->msg.msg_name = (struct sockaddr *)&u->source_addr;
    u->msg.msg_namelen = sizeof(u->source_addr);

    return ESP_OK;
}

static void sock_shutdown(rtp_udp_t *u) {
    assert(u != NULL);

    assert(u->sock != -1);
    ESP_LOGI(TAG, "Shutting down socket");
    shutdown(u->sock, 0);
    close(u->sock);
    u->sock = -1;
}

static esp_err_t sock_receive(rtp_udp_t *u) {
    u->rx_sz = 0;

    ESP_LOGD(TAG, "Waiting for data");
    assert(u->sock >= 0);
    const ptrdiff_t sz = recvmsg(u->sock, &u->msg, 0);
    if (sz < 0) {
        ESP_LOGE(TAG, "recvmsg() failed: errno %d", errno);
        return ESP_FAIL;
    }
    u->rx_sz = sz;

    // Get sender IP as string.
    char addr_str[16];
    assert(u->source_addr.ss_family == PF_INET);
    inet_ntoa_r(((struct sockaddr_in *)&u->source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
    ESP_LOGD(TAG, "Received %d bytes from %s", sz, addr_str);

    return ESP_OK;
}

static void jpeg_frame_cb(const rtp_jpeg_frame_t *frame, void *userdata) {
    assert(frame != NULL);
    rtp_udp_t *u = (rtp_udp_t *)userdata;
    assert(u != NULL);

    const int success = xQueueOverwrite(u->out, frame->jpeg_data);
    ESP_LOGD(TAG, "Frame %dx%d ts=%" PRIu32 " posted to queue success=%d", frame->width,
             frame->height, frame->timestamp, success);
}

ptrdiff_t rtp_udp_recv_task_approx_stack_sz() {
    return sizeof(rtp_udp_t) + sizeof(rtp_jpeg_session_t) + sizeof(rtp_jitbuf_t) + 3 * 1024;
}

void rtp_udp_recv_task(void *pvParameters) {
    ESP_LOGI(TAG, "Started");

    rtp_udp_t u = {0};
    u.sock = -1;
    assert(pvParameters != NULL);
    u.out = (QueueHandle_t)pvParameters;

    while (1) {
        const esp_err_t err = sock_bind_prepare(&u);
        if (err != ESP_OK) {
            continue;
        }

        ESP_LOGD(TAG, "Starting receive loop");

        bool sess_initialized = false;
        rtp_jpeg_session_t sess = {0};
        rtp_jitbuf_t jitbuf = {0};

        while (1) {
            const esp_err_t err2 = sock_receive(&u);
            if (err2 != ESP_OK) {
                ESP_LOGW(TAG, "sock_receive() failed: %d", err2);
                break;
            }

            if (!sess_initialized) {
                // Parse RTP header.
                uint16_t sequence_number = 0;
                uint32_t ssrc = 0;
                if (partial_parse_rtp_packet((uint8_t *)u.rx_buf, u.rx_sz, &sequence_number,
                                             &ssrc) != ESP_OK) {
                    ESP_LOGD(TAG, "Failed to parse RTP header");
                    continue;
                }

                // Try to initialize session.
                ESP_LOGI(TAG, "Starting session with ssrc=%" PRIu32, ssrc);
                init_rtp_jitbuf(ssrc, &jitbuf);
                init_rtp_jpeg_session(ssrc, jpeg_frame_cb, &u, &sess);
                sess_initialized = true;
            }

            if (rtp_jitbuf_feed(&jitbuf, (uint8_t *)u.rx_buf, u.rx_sz) != ESP_OK) {
                ESP_LOGD(TAG, "Failed to feed RTP packet to jitbuf");
                continue;
            }

            // Feed from jitbuf to jpeg session.
            ptrdiff_t retr_sz = 0;
            while ((retr_sz = rtp_jitbuf_retrieve(&jitbuf, (uint8_t *)u.rx_buf, sizeof(u.rx_buf))) >
                   0) {
                rtp_packet_t packet;
                if (parse_rtp_packet((uint8_t *)u.rx_buf, retr_sz, &packet) != ESP_OK) {
                    ESP_LOGD(TAG, "Failed to parse RTP header");
                    continue;
                }

                ESP_LOGD(TAG, "Feed to JPEG session");
                esp_err_t err3 = rtp_jpeg_session_feed(&sess, &packet);
                if (err3 != ESP_OK) {
                    ESP_LOGI(TAG, "Failed to feed RTP packet to jpeg_session %d", err3);
                    continue;
                }
            }
        }

        ESP_LOGD(TAG, "Reset socket");
        sock_shutdown(&u);
    }

    vTaskDelete(NULL);
}
