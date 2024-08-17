#pragma once

#include <esp_err.h>
#include <stdint.h>

#include "lcd.h"

// All struct members are private to the implementation.
typedef struct jpeg_decoder_t {
    const uint8_t *data;
    ptrdiff_t data_max_sz;
    ptrdiff_t read_offset;
    lcd_t *lcd;

    // Buffer a chunk of display_w_px * block_sz_px of pixels before writing to the display.
    uint16_t *px_buf;
    ptrdiff_t px_buf_sz;

    // Working memory for tjpgd.
    void *work;
} jpeg_decoder_t;

esp_err_t init_jpeg_decoder(const ptrdiff_t data_max_sz, lcd_t *lcd, jpeg_decoder_t *out);
esp_err_t jpeg_decoder_decode_to_lcd(jpeg_decoder_t *d, const uint8_t *data);
void jpeg_decoder_destroy(jpeg_decoder_t *d);
