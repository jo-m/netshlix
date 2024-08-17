#include <arpa/inet.h>
#include <assert.h>
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

void jpeg_frame_cb(const rtp_jpeg_frame_t *frame, void *userdata __attribute__((unused))) {
    assert(frame != NULL);
    ESP_LOGI(TAG, "========== FRAME %dx%d %u ==========", frame->width, frame->height,
             frame->timestamp);

    static int fcount = 0;
    char fname[128] = {0};
    snprintf(fname, sizeof(fname), "frames/frame_%010d.jpeg", fcount++);

    FILE *f = fopen(fname, "w");
    assert(f != NULL);
    ptrdiff_t written = fwrite(frame->jpeg_data, 1, frame->jpeg_data_sz, f);
    assert(written == frame->jpeg_data_sz);
    fclose(f);
}

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

    rtp_jpeg_session_t sess = {0};
    rtp_jitbuf_t jitbuf = {0};

    while (1) {
        // Receive packet.
        addr_size = sizeof client_addr;
        memset(buf, 0, sizeof(buf));
        const ptrdiff_t sz =
            recvfrom(sockfd, buf, MAX_BUFFER, 0, (struct sockaddr *)&client_addr, &addr_size);
        ESP_LOGI(TAG, "Received %ld bytes on port %d from %s", sz, client_addr.sin_port,
                 inet_ntoa(client_addr.sin_addr));

        // Parse RTP header.
        uint16_t sequence_number = 0;
        uint32_t ssrc = 0;
        if (partial_parse_rtp_packet((uint8_t *)buf, sz, &sequence_number, &ssrc) != ESP_OK) {
            ESP_LOGI(TAG, "Failed to parse RTP header");
            continue;
        }

        if (sess.ssrc == 0) {
            // Try to initialize session.
            ESP_LOGI(TAG, "Starting session with ssrc=%u", ssrc);
            init_rtp_jitbuf(ssrc, &jitbuf);
            init_rtp_jpeg_session(ssrc, jpeg_frame_cb, NULL, &sess);
        }

        if (rtp_jitbuf_feed(&jitbuf, (uint8_t *)buf, sz) != ESP_OK) {
            ESP_LOGI(TAG, "Failed to feed RTP packet to jitbuf");
        }

        // Feed from jitbuf to jpeg session.
        uint8_t retr_buf[MAX_BUFFER];
        ptrdiff_t retr_sz = 0;
        while ((retr_sz = rtp_jitbuf_retrieve(&jitbuf, retr_buf, sizeof(retr_buf))) > 0) {
            rtp_packet_t packet;
            if (parse_rtp_packet(retr_buf, retr_sz, &packet) != ESP_OK) {
                ESP_LOGI(TAG, "Failed to parse RTP header");
                continue;
            }

            ESP_LOGI(TAG, "Feed to JPEG session");
            if (rtp_jpeg_session_feed(&sess, &packet) != ESP_OK) {
                ESP_LOGI(TAG, "Failed to feed RTP packet to jpeg_session");
            }
        }
    }

    return 0;
}

// Dummy function used only to measure stack size via `-fstack-usage`.
void stack_usage() {
    uint8_t buf[1400];
    rtp_jpeg_session_t sess = {0};
    rtp_jitbuf_t jitbuf = {0};

    memset(buf, 0, sizeof(buf));

    // Parse RTP header.
    uint16_t sequence_number = 0;
    uint32_t ssrc = 0;
    partial_parse_rtp_packet((uint8_t *)buf, sizeof(buf), &sequence_number, &ssrc);

    // Try to initialize session.
    ESP_LOGI(TAG, "Starting session with ssrc=%u", ssrc);
    init_rtp_jitbuf(ssrc, &jitbuf);
    init_rtp_jpeg_session(ssrc, jpeg_frame_cb, NULL, &sess);

    rtp_jitbuf_feed(&jitbuf, (uint8_t *)buf, sizeof(buf));
    rtp_jitbuf_retrieve(&jitbuf, buf, sizeof(buf));
    rtp_packet_t packet;
    parse_rtp_packet(buf, sizeof(buf), &packet);
    rtp_jpeg_session_feed(&sess, &packet);
}
