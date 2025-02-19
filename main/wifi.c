#include "wifi.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <freertos/FreeRTOS.h>
#pragma GCC diagnostic pop
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <lwip/err.h>
#include <lwip/sys.h>

#define WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#define WIFI_H2E_IDENTIFIER "PASSWORD IDENTIFIER"

// FreeRTOS event group to signal when we are connected.
static EventGroupHandle_t s_wifi_event_group;

static bool have_ip_info = false;
static esp_netif_ip_info_t ip_info = {0};

// The event group allows multiple bits for each event, but we only care about two events:
// - we are connected to the AP with an IP.
// - we failed to connect after the maximum amount of retries.
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char* TAG = "sta";

static int s_retry_num = 0;

esp_err_t get_ip_info(esp_netif_ip_info_t* ip_info_out) {
    assert(ip_info_out != NULL);
    if (have_ip_info) {
        memcpy(ip_info_out, &ip_info, sizeof(*ip_info_out));
        return ESP_OK;
    }
    return ESP_ERR_INVALID_STATE;
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                          void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "esp_wifi_connect() initial");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                if (s_retry_num < CONFIG_SMALLTV_WIFI_MAXIMUM_RETRY) {
                    s_retry_num++;
                    ESP_LOGI(TAG, "esp_wifi_connect() retry=%d", s_retry_num);
                    esp_wifi_connect();
                } else {
                    ESP_LOGI(TAG, "esp_wifi_connect() giving up");
                    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                }
                break;
            default:
                ESP_LOGV(TAG, "Received unhandled WIFI_EVENT event_id=%ld", event_id);
                break;
        }
        return;
    }

    if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP: {
                ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
                ESP_LOGI(TAG, "Got ip:" IPSTR, IP2STR(&event->ip_info.ip));
                memcpy(&ip_info, &event->ip_info, sizeof(ip_info));
                have_ip_info = true;
                xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            } break;
            case IP_EVENT_STA_LOST_IP: {
                ESP_LOGE(TAG, "Lost IP, resetting");
                fflush(stdout);
                fflush(stderr);
                esp_restart();
            } break;
            default:
                ESP_LOGV(TAG, "Received unhandled IP_EVENT event_id=%ld", event_id);
                break;
        }
        return;
    }

    ESP_LOGI(TAG, "Received unhandled event_base=%p event_id=%ld", event_base, event_id);
}

void init_wifi_sta() {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id = {0};
    esp_event_handler_instance_t instance_got_ip = {0};
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta =
            {
                .ssid = CONFIG_SMALLTV_WIFI_SSID,
                .password = CONFIG_SMALLTV_WIFI_PASSWORD,
                .scan_method = WIFI_ALL_CHANNEL_SCAN,
                .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
                .threshold.authmode = WIFI_SCAN_AUTH_MODE_THRESHOLD,
                .failure_retry_cnt = 2,
                .sae_pwe_h2e = WIFI_SAE_MODE,
                .sae_h2e_identifier = WIFI_H2E_IDENTIFIER,
            },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed
    // for the maximum number of re-tries (WIFI_FAIL_BIT).
    // The bits are set by event_handler().
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);

    // xEventGroupWaitBits() returns the bits before the call returned, hence we can test which
    // event actually happened.
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID: %s", CONFIG_SMALLTV_WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGW(TAG, "Failed to connect to SSID:%s ", CONFIG_SMALLTV_WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "Unexpected event");
    }
}
