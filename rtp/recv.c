#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "fakesp.h"
#include "rtp.h"
#include "rtp_jpeg.h"

static const char *TAG = "main";

#define PORT 1234
#define MAX_BUFFER 65536

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buf[MAX_BUFFER];
    socklen_t addr_size;

    // Create socket.
    if ((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("cannot create socket");
        return 0;
    }

    // Bind to port.
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        return 0;
    }

    uint32_t ssrc = 0;
    rtp_jpeg_session_t sess = {0};
    rtp_jitbuf_t jitbuf = {0};

    while (1) {
        // Receive packet.
        addr_size = sizeof client_addr;
        memset(buf, 0, sizeof(buf));
        const size_t sz =
            recvfrom(sockfd, buf, MAX_BUFFER, 0, (struct sockaddr *)&client_addr, &addr_size);
        printf("Received %ld bytes on port %d from %s\n", sz, client_addr.sin_port,
               inet_ntoa(client_addr.sin_addr));

        // Parse RTP header.
        rtp_header_t header;
        {
            const esp_err_t success = parse_rtp_header((uint8_t *)buf, sz, &header);
            if (success != ESP_OK) {
                ESP_LOGI(TAG, "Failed to parse RTP header");
                continue;
            }
        }

        // Try to initialize session.
        if (ssrc == 0) {
            ssrc = header.ssrc;
            ESP_LOGI(TAG, "Starting session with ssrc=%u\n", ssrc);
            init_rtp_jpeg_session(ssrc, &sess);
            init_rtp_jitbuf(ssrc, RTP_PT_CLOCKRATE_JPEG, &jitbuf);
        }

        if (ssrc != 0) {
            // const esp_err_t success = rtp_jpeg_session_feed(&sess, header);
            // if (success != ESP_OK) {
            //     ESP_LOGI(TAG, "Failed to feed RTP packet");
            //     continue;
            // }

            const esp_err_t success2 = rtp_jitbuf_feed(&jitbuf, (uint8_t *)buf, sz);
            if (success2 != ESP_OK) {
                ESP_LOGI(TAG, "Failed to feed RTP packet");
                continue;
            }

            uint8_t rxbuf[MAX_BUFFER];
            ptrdiff_t len = 0;
            while ((len = rtp_jitbuf_retrieve(&jitbuf, rxbuf, sizeof(rxbuf))) > 0) {
                ESP_LOGI(TAG, "Got packet");
            }
        }
    }

    return 0;
}
