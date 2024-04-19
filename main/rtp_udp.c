#include "rtp_udp.h"

#include <string.h>
#include <sys/param.h>
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

static esp_err_t socket_bind(int *sock) {
    assert(sock != NULL);

    struct sockaddr_in dest_addr = {0};
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(CONFIG_SMALLTV_RTP_PORT);

    *sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (*sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Socket created");

    const int enable = 1;
    lwip_setsockopt(*sock, IPPROTO_IP, IP_PKTINFO, &enable, sizeof(enable));

    // Set timeout
    struct timeval timeout = {0};
    timeout.tv_sec = 2;  // TODO: configure
    timeout.tv_usec = 0;
    setsockopt(*sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

    const int err = bind(*sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", CONFIG_SMALLTV_RTP_PORT);

    return ESP_OK;
}

void rtp_udp_recv_task(void *pvParameters __attribute__((unused))) {
    while (1) {
        int sock = 0;
        const esp_err_t err = socket_bind(&sock);
        if (err != ESP_OK) {
            continue;
        }

        struct sockaddr_storage source_addr={0};
        socklen_t socklen = sizeof(source_addr);

        char rx_buffer[128];
        struct iovec iov={0};
        iov.iov_base = rx_buffer;
        iov.iov_len = sizeof(rx_buffer);

        struct msghdr msg={0};
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_name = (struct sockaddr *)&source_addr;
        msg.msg_namelen = socklen;

        while (1) {
            ESP_LOGI(TAG, "Waiting for data");
            int len = recvmsg(sock, &msg, 0);
            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }

            // Data received
            char addr_str[CONFIG_SMALLTV_UDP_MTU_BYTES];
            assert(source_addr.ss_family == PF_INET);
            // Get the sender's ip address as string
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str,
                        sizeof(addr_str) - 1);

            rx_buffer[len] = 0;  // Null-terminate whatever we received and treat like a string...
            ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
            ESP_LOGI(TAG, "%s", rx_buffer);

            const int err2 = sendto(sock, rx_buffer, len, 0, (struct sockaddr *)&source_addr,
                                    sizeof(source_addr));
            if (err2 < 0) {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                break;
            }
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }

    vTaskDelete(NULL);
}
