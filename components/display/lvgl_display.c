#include "lvgl_display.h"

#include <assert.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <stddef.h>

#include "lcd.h"

static const char *TAG = "display";

static void lcd_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    lcd_t *lcd = (lcd_t *)lv_display_get_user_data(disp);
    int x1 = area->x1;
    int x2 = area->x2;
    int y1 = area->y1;
    int y2 = area->y2;

    ESP_LOGD(TAG, "lcd_draw_start() lvgl x1=%d y1=%d x2=%d y2=%d", x1, y1, x2, y2);
    lcd_draw_start(lcd, x1, y1, x2, y2, px_map);
}

static void lcd_wait_cb(lv_display_t *disp) {
    lcd_t *lcd = (lcd_t *)lv_display_get_user_data(disp);
    lcd_draw_wait_finished(lcd);
}

static uint32_t lcd_lvgl_tick_get_cb() { return esp_timer_get_time() / 1000; }

ptrdiff_t lvgl_display_get_buf_sz() {
    const ptrdiff_t buf_sz = SMALLTV_LCD_X_RES * SMALLTV_LCD_COLOR_DEPTH_BYTE * 24;
    _Static_assert(
        (SMALLTV_LCD_X_RES * SMALLTV_LCD_Y_RES * SMALLTV_LCD_COLOR_DEPTH_BYTE) % buf_sz == 0,
        "Screen size should be divisible by LVGL screen buffer size");
    _Static_assert(
        (buf_sz * 10) >= (SMALLTV_LCD_X_RES * SMALLTV_LCD_Y_RES * SMALLTV_LCD_COLOR_DEPTH_BYTE),
        "LVGL recommends display buffer size to be at least 1/10 of display.");
    return buf_sz;
}

void init_lvgl_display(lcd_t *lcd, uint8_t *buf, ptrdiff_t buf_sz, lv_display_t **disp_out) {
    assert(lcd != NULL);
    assert(buf != NULL);
    assert(buf_sz == lvgl_display_get_buf_sz());

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    lv_tick_set_cb(lcd_lvgl_tick_get_cb);

    ESP_LOGI(TAG, "Initialize LVGL display");
    lv_display_t *disp = lv_display_create(SMALLTV_LCD_X_RES, SMALLTV_LCD_Y_RES);
    assert(disp != NULL);
    lv_display_set_user_data(disp, (void *)lcd);
    lv_display_set_flush_wait_cb(disp, lcd_wait_cb);
    lv_display_set_flush_cb(disp, lcd_flush_cb);
    lv_display_set_buffers(disp, buf, NULL, buf_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(disp, SMALLTV_LCD_COLOR_FORMAT);

    assert(disp_out != NULL);
    *disp_out = disp;
}
