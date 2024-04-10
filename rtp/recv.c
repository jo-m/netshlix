#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "rtp.h"
#include "rtp_jpeg.h"

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

    while (1) {
        // Receive packets
        addr_size = sizeof client_addr;
        const size_t received =
            recvfrom(sockfd, buffer, MAX_BUFFER, 0, (struct sockaddr *)&client_addr, &addr_size);
        printf("Received %ld bytes on port %d from %s\n", received, client_addr.sin_port,
               inet_ntoa(client_addr.sin_addr));

        rtp_header_t header;
        ptrdiff_t parsed = parse_rtp_header((uint8_t *)buffer, received, &header);
        if (parsed > 0) {
            printf(
                "RTP Header: parsed=%ld v=%hhu e=%hhu c=%hhu mark=%hhu pt=%hhu seq=%hu ts=%hu "
                "ssrc=%u\n",
                parsed, header.version, header.extension, header.csrc_count, header.marker,
                header.payload_type, header.sequence_number, header.timestamp, header.ssrc);
        }

        rtp_header_jpeg_t header_jpeg;
        parsed = parse_rtp_jpeg_header((uint8_t *)(buffer + parsed), received, &header_jpeg);
        if (parsed > 0) {
            printf("RTP JPEG Header: parsed=%ld tpsp=%hhu fofs=%u tp=%hhu q=%hhu sz=%hux%hu\n",
                   parsed, header_jpeg.type_specific, header_jpeg.fragment_offset, header_jpeg.type,
                   header_jpeg.q, header_jpeg.width, header_jpeg.height);
        }

        memset(buffer, 0, sizeof(buffer));  // Clear buffer
    }

    return 0;
}
