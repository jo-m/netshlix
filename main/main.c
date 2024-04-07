#include <math.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "sdkconfig.h"

static const char *TAG = "example";

#define SMALLTV_PRO_LCD_H_RES 240
#define SMALLTV_PRO_LCD_V_RES 240
#define SMALLTV_PRO_LCD_COLOR_FORMAT LV_COLOR_FORMAT_RGB565
#define SMALLTV_PRO_LCD_COLOR_DEPTH_BIT 16
#define SMALLTV_PRO_LCD_COLOR_DEPTH_BYTE (SMALLTV_PRO_LCD_COLOR_DEPTH_BIT / 8)

#define SMALLTV_PRO_LCD_MAX_TRANSFER_LINES \
    80  // SMALLTV_PRO_LCD_V_RES should be a multiple of this.
#define SMALLTV_PRO_LCD_HOST SPI2_HOST
#define SMALLTV_PRO_LCD_PX_CLK_HZ (40 * 1000 * 1000)

#define SMALLTV_PRO_LCD_MOSI GPIO_NUM_23
#define SMALLTV_PRO_LCD_SCLK GPIO_NUM_18
#define SMALLTV_PRO_LCD_DC GPIO_NUM_2
#define SMALLTV_PRO_LCD_RST GPIO_NUM_4

#define SMALLTV_PRO_LCD_CMD_BITS 8
#define SMALLTV_PRO_LCD_PARAM_BITS 8

#define SMALLTV_PRO_LCD_BL_PIN 25
#define SMALLTV_PRO_LCD_BL_PWM_CHANNEL 0  // TODO: use
#define SMALLTV_PRO_LCD_BL_ON_LEVEL 0
#define SMALLTV_PRO_LCD_BL_OFF_LEVEL 1

#define SMALLTV_PRO_TOUCH_PIN GPIO_NUM_32

static void anim_x_cb(void *var, int32_t v) { lv_obj_set_x(var, v); }

static void anim_size_cb(void *var, int32_t v) { lv_obj_set_size(var, v * 4, v * 4); }

void lvgl_dummy_ui(lv_display_t *disp) {
    lv_obj_t *scr = lv_display_get_screen_active(disp);

    // Text label
    lv_obj_t *label = lv_label_create(scr);

    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(label, "Hello Espressif, Hello LVGL. This must be a bit longer to scoll.");

    lv_obj_set_width(label, 240);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);

    // Animation
    lv_obj_t *obj = lv_obj_create(scr);
    lv_obj_set_style_bg_color(obj, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);

    lv_obj_align(obj, LV_ALIGN_LEFT_MID, 10, 0);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, 10, 40);
    lv_anim_set_duration(&a, 1000);
    lv_anim_set_playback_delay(&a, 100);
    lv_anim_set_playback_duration(&a, 300);
    lv_anim_set_repeat_delay(&a, 500);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);

    lv_anim_set_exec_cb(&a, anim_size_cb);
    lv_anim_start(&a);
    lv_anim_set_exec_cb(&a, anim_x_cb);
    lv_anim_set_values(&a, 10, 240);
    lv_anim_start(&a);
}

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

void app_main(void) {
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {.mode = GPIO_MODE_OUTPUT,
                                    .pin_bit_mask = 1ULL << SMALLTV_PRO_LCD_BL_PIN};
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(SMALLTV_PRO_LCD_BL_PIN, SMALLTV_PRO_LCD_BL_OFF_LEVEL);

    spi_bus_config_t buscfg = {
        .sclk_io_num = SMALLTV_PRO_LCD_SCLK,
        .mosi_io_num = SMALLTV_PRO_LCD_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SMALLTV_PRO_LCD_MAX_TRANSFER_LINES * SMALLTV_PRO_LCD_H_RES *
                           SMALLTV_PRO_LCD_COLOR_DEPTH_BYTE,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SMALLTV_PRO_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = SMALLTV_PRO_LCD_DC,
        .cs_gpio_num = -1,  // Not connected, tied to GND
        .pclk_hz = SMALLTV_PRO_LCD_PX_CLK_HZ,
        .lcd_cmd_bits = SMALLTV_PRO_LCD_CMD_BITS,
        .lcd_param_bits = SMALLTV_PRO_LCD_PARAM_BITS,
        .spi_mode = 3,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SMALLTV_PRO_LCD_HOST,
                                             &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = SMALLTV_PRO_LCD_RST,
        .rgb_ele_order = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = SMALLTV_PRO_LCD_COLOR_DEPTH_BIT,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));

    ESP_LOGI(TAG, "Turn on LCD backlight");
    ESP_ERROR_CHECK(gpio_set_level(SMALLTV_PRO_LCD_BL_PIN, SMALLTV_PRO_LCD_BL_ON_LEVEL));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    lv_tick_set_cb(lcd_lvgl_tick_get_cb);
    const size_t buf_sz =
        SMALLTV_PRO_LCD_H_RES * SMALLTV_PRO_LCD_V_RES * SMALLTV_PRO_LCD_COLOR_DEPTH_BYTE / 3;
    assert(buscfg.max_transfer_sz == buf_sz);
    ESP_LOGI(TAG, "Free memory: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Buf size: %u", buf_sz);
    lv_color_t *buf1 = heap_caps_malloc(buf_sz, MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = heap_caps_malloc(buf_sz, MALLOC_CAP_DMA);
    assert(buf2);

    ESP_LOGI(TAG, "Initialize LVGL display");
    lv_display_t *disp = lv_display_create(SMALLTV_PRO_LCD_H_RES, SMALLTV_PRO_LCD_V_RES);
    lv_display_set_user_data(disp, (void *)panel_handle);
    lv_display_set_flush_cb(disp, lcd_flush_cb);
    lv_display_set_buffers(disp, buf1, buf2, buf_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(disp, SMALLTV_PRO_LCD_COLOR_FORMAT);

    ESP_LOGI(TAG, "Display LVGL animation");
    lvgl_dummy_ui(disp);

    while (1) {
        uint32_t time_till_next_ms = lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(time_till_next_ms));
    }
}
