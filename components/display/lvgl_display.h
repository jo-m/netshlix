#pragma once

#include "lcd.h"
#include "lvgl.h"

// If buf_out and buf_sz_out are non-NULL, also returns the pixel buffer allocated for LVGL (with
// DMA capabilities), so that it can be reused for different purposes when LVGL is not using it.
void init_lvgl_display(lcd_t *lcd, lv_display_t **disp_out, uint8_t **buf_out,
                       ptrdiff_t *buf_sz_out);
