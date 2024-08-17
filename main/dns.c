#include "dns.h"

#include <esp_log.h>
#include <esp_mac.h>
#include <mdns.h>

static const char *TAG = "mdns";

static char *generate_hostname(void) {
    uint8_t mac[6];
    char *hostname;
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (-1 == asprintf(&hostname, "%s-%02X%02X%02X", CONFIG_SMALLTV_MDNS_HOSTNAME, mac[3], mac[4],
                       mac[5])) {
        abort();
    }
    return hostname;
}

void mdns_svr_init() {
    char *hostname = generate_hostname();

    // initialize mDNS
    ESP_ERROR_CHECK(mdns_init());
    // set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK(mdns_hostname_set(hostname));
    ESP_LOGI(TAG, "mdns hostname set to: [%s]", hostname);
    // set default mDNS instance name
    ESP_ERROR_CHECK(mdns_instance_name_set(CONFIG_SMALLTV_MDNS_INSTANCE));

    // structure with TXT records
    mdns_txt_item_t service_txt_data[2] = {{"format", "rfc2435"}, {"dispsize", "240x240"}};

    // initialize service
    ESP_ERROR_CHECK(
        mdns_service_add("RTP/JPEG display", "_rtp", "_udp", 1234, service_txt_data, 2));

    free(hostname);
}
