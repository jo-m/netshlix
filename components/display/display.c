#include "display.h"

#include <assert.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_ops.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>

#include "lcd.h"

static const char *TAG = "display";

static void lcd_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    int x1 = area->x1;
    int x2 = area->x2;
    int y1 = area->y1;
    int y2 = area->y2;
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1, y2 + 1, px_map));

    lv_display_flush_ready(disp);
}

static uint32_t lcd_lvgl_tick_get_cb() { return esp_timer_get_time() / 1000; }

esp_err_t display_init(esp_lcd_panel_handle_t panel_handle, lv_display_t **disp_out) {
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    lv_tick_set_cb(lcd_lvgl_tick_get_cb);

    ESP_LOGI(TAG, "Allocate display buffers");
    // Note that this is equal to API buscfg.max_transfer_sz.
    const size_t buf_sz =
        SMALLTV_PRO_LCD_H_RES * SMALLTV_PRO_LCD_V_RES * SMALLTV_PRO_LCD_COLOR_DEPTH_BYTE / 3;
    ESP_LOGI(TAG, "Free memory: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Buf size: %u", buf_sz);
    lv_color_t *buf1 = heap_caps_malloc(buf_sz, MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = heap_caps_malloc(buf_sz, MALLOC_CAP_DMA);
    assert(buf2);

    ESP_LOGI(TAG, "Initialize LVGL display");

    lv_display_t *disp = lv_display_create(SMALLTV_PRO_LCD_H_RES, SMALLTV_PRO_LCD_V_RES);
    assert(disp != NULL);
    lv_display_set_user_data(disp, (void *)panel_handle);
    lv_display_set_flush_cb(disp, lcd_flush_cb);
    lv_display_set_buffers(disp, buf1, buf2, buf_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(disp, SMALLTV_PRO_LCD_COLOR_FORMAT);

    assert(disp_out != NULL);
    *disp_out = disp;

    return ESP_OK;
}
