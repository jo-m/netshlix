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

    // tjpgd state.
    void *work;
    struct JDEC *jdec;
} jpeg_decoder_t;

// The pixel buffer needs to be compatible with the tjpgd block size, and DMA capable.
esp_err_t init_jpeg_decoder(lcd_t *lcd, uint8_t *px_buf, ptrdiff_t px_buf_sz, jpeg_decoder_t *out);
esp_err_t jpeg_decoder_decode_to_lcd(jpeg_decoder_t *d, const uint8_t *data,
                                     const ptrdiff_t data_max_sz);
void jpeg_decoder_destroy(jpeg_decoder_t *d);
