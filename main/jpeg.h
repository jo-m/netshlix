#pragma once

#include <esp_err.h>
#include <esp_lcd_types.h>

#include "rtp_udp.h"

esp_err_t decode_jpeg(const uint8_t *data, const ptrdiff_t data_max_sz,
                      esp_lcd_panel_handle_t panel_handle);
