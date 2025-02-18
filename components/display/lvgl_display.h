#pragma once

#include "lcd.h"
#include "lvgl.h"

// Returns the desired size of the screen buffer to pass to init_lvgl_display().
ptrdiff_t lvgl_display_get_buf_sz();

// Buffer `buf` must be of size `buf_sz` == `lvgl_display_get_buf_sz()`,
// and must be allocated with `MALLOC_CAP_DMA`.
void init_lvgl_display(lcd_t *lcd, uint8_t *buf, ptrdiff_t buf_sz, lv_display_t **disp_out);
