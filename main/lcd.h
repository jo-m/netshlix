#pragma once

#include <esp_err.h>
#include <esp_lcd_types.h>

#include "lvgl.h"

#define SMALLTV_PRO_LCD_H_RES 240
#define SMALLTV_PRO_LCD_V_RES 240
#define SMALLTV_PRO_LCD_COLOR_FORMAT LV_COLOR_FORMAT_RGB565
#define SMALLTV_PRO_LCD_COLOR_DEPTH_BIT 16
#define SMALLTV_PRO_LCD_COLOR_DEPTH_BYTE (SMALLTV_PRO_LCD_COLOR_DEPTH_BIT / 8)

#define SMALLTV_PRO_LCD_MAX_TRANSFER_LINES \
    80  // SMALLTV_PRO_LCD_V_RES should be a multiple of this.
#define SMALLTV_PRO_LCD_PX_CLK_HZ (40 * 1000 * 1000)

#define SMALLTV_PRO_LCD_SPI_HOST SPI2_HOST
#define SMALLTV_PRO_LCD_SPI_MOSI_PIN GPIO_NUM_23
#define SMALLTV_PRO_LCD_SPI_SCLK_PIN GPIO_NUM_18
#define SMALLTV_PRO_LCD_SPI_DC_PIN GPIO_NUM_2
#define SMALLTV_PRO_LCD_RST_PIN GPIO_NUM_4

#define SMALLTV_PRO_LCD_CMD_BITS 8
#define SMALLTV_PRO_LCD_PARAM_BITS 8

#define SMALLTV_PRO_LCD_BL_PIN 25
#define SMALLTV_PRO_LCD_BL_PWM_CHANNEL 0  // TODO: use
#define SMALLTV_PRO_LCD_BL_ON_LEVEL 0
#define SMALLTV_PRO_LCD_BL_OFF_LEVEL 1

esp_err_t lcd_init(esp_lcd_panel_handle_t *panel_handle_out);
esp_err_t backlight_onoff(bool enable);
