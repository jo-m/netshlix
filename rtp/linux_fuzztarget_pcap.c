#include <arpa/inet.h>
#include <assert.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "fakesp.h"
#include "rtp.h"
#include "rtp_jpeg.h"

__attribute__((unused)) static const char *TAG = "fuzz";

#define PORT 1234
#define MAX_BUFFER 65536

void jpeg_frame_cb(const rtp_jpeg_frame_t frame __attribute__((unused)),
                   void *userdata __attribute__((unused))) {
    ESP_LOGE(TAG, "========== FRAME %dx%d %u ==========", frame.width, frame.height,
             frame.timestamp);
}

typedef struct udp_packet_t {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t *payload;
    uint16_t payload_length;
} udp_packet_t;

struct udp_packet_t unwrap_udp_packet(const uint8_t *packet) {
    struct ip *ip_header;
    struct udphdr *udp_header;
    unsigned int ip_header_length;
    struct udp_packet_t result = {0};

    // Skip the Ethernet header
    ip_header = (struct ip *)(packet + 14);
    ip_header_length = ip_header->ip_hl * 4;

    // Check if it's a UDP packet
    if (ip_header->ip_p != IPPROTO_UDP) {
        result.payload = NULL;
        return result;  // Not a UDP packet
    }

    // Skip the IP header
    udp_header = (struct udphdr *)(packet + 14 + ip_header_length);

    // Verify the destination port
    if (ntohs(udp_header->uh_dport) != PORT) {
        result.payload = NULL;
        return result;  // Not destined for the specified port
    }

    // Extract UDP information
    result.src_port = ntohs(udp_header->uh_sport);
    result.dst_port = ntohs(udp_header->uh_dport);
    result.src_ip = ip_header->ip_src.s_addr;
    result.dst_ip = ip_header->ip_dst.s_addr;
    result.payload_length = ntohs(udp_header->uh_ulen) - sizeof(struct udphdr);
    result.payload = (uint8_t *)(packet + 14 + ip_header_length + sizeof(struct udphdr));

    return result;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <pcap_file>\n", argv[0]);
        return 1;
    }

    // Open the pcap file
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle = pcap_open_offline(argv[1], errbuf);
    if (handle == NULL) {
        printf("Error opening pcap file: %s\n", errbuf);
        return 1;
    }

    rtp_jpeg_session_t sess = {0};
    rtp_jitbuf_t jitbuf = {0};

    // Loop through packets and process
    struct pcap_pkthdr pcapheader;
    const uint8_t *pcapbuf;
    while ((pcapbuf = pcap_next(handle, &pcapheader)) != NULL) {
        const udp_packet_t udp = unwrap_udp_packet(pcapbuf);

        // Parse RTP header.
        uint16_t sequence_number = 0;
        uint32_t ssrc = 0;
        if (partial_parse_rtp_packet(udp.payload, udp.payload_length, &sequence_number, &ssrc) !=
            ESP_OK) {
            ESP_LOGI(TAG, "Failed to parse RTP header");
            continue;
        }

        ESP_LOGI(TAG, "Got UDP packet %hu %hu", udp.dst_port, udp.payload_length);
        if (sess.ssrc == 0) {
            // Try to initialize session.
            ESP_LOGI(TAG, "Starting session with ssrc=%u", ssrc);
            init_rtp_jitbuf(ssrc, &jitbuf);
            init_rtp_jpeg_session(ssrc, jpeg_frame_cb, NULL, &sess);
        }

        if (rtp_jitbuf_feed(&jitbuf, udp.payload, udp.payload_length) != ESP_OK) {
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
            if (rtp_jpeg_session_feed(&sess, packet) != ESP_OK) {
                ESP_LOGI(TAG, "Failed to feed RTP packet to jpeg_session");
            }
        }
    }

    // Close the pcap file
    pcap_close(handle);

    return 0;
}
