#pragma once

#include "lcd.h"
#include "lvgl.h"

void init_lvgl_display(lcd_t *lcd, lv_display_t **disp_out);
