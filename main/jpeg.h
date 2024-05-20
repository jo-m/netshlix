#pragma once

#include <esp_err.h>
#include <stdint.h>

#include "lcd.h"

esp_err_t jpeg_decode_to_lcd(const uint8_t *data, const ptrdiff_t data_max_sz, lcd_t *lcd);
