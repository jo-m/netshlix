#pragma once

#include <esp_err.h>
#include <esp_netif_types.h>
#include <esp_wifi_types.h>

void init_wifi_sta();
esp_err_t get_ip_info(esp_netif_ip_info_t *ip_info_out);
