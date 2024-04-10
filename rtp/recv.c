#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "fakesp.h"
#include "rtp.h"
#include "rtp_jpeg.h"
#include "time.h"

static const char *TAG = "main";

#define PORT 1234
#define MAX_BUFFER 65536

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[MAX_BUFFER];
    socklen_t addr_size;

    // Create socket
    if ((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("cannot create socket");
        return 0;
    }

    // Bind to port
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

    while (1) {
        // Receive packets
        addr_size = sizeof client_addr;
        const size_t received =
            recvfrom(sockfd, buffer, MAX_BUFFER, 0, (struct sockaddr *)&client_addr, &addr_size);
        printf("Received %ld bytes on port %d from %s\n", received, client_addr.sin_port,
               inet_ntoa(client_addr.sin_addr));

        // Try to initialize session
        if (ssrc == 0) {
            rtp_header_t header;
            ptrdiff_t parsed = parse_rtp_header((uint8_t *)buffer, received, &header);
            if (parsed > 0) {
                ssrc = header.ssrc;
            }
            create_rtp_jpeg_session(ssrc, &sess);

            ESP_LOGI(TAG, "Starting session with ssrc=%u\n", ssrc);
        }

        rtp_jpeg_session_feed(&sess, (uint8_t *)buffer, received, micros());

        memset(buffer, 0, sizeof(buffer));
    }

    return 0;
}
