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
#define SMALLTV_PRO_LCD_COLOR_DEPTH_BIT 16  // RGB565 == LV_COLOR_FORMAT_RGB565
#define SMALLTV_PRO_LCD_COLOR_DEPTH_BYTE (SMALLTV_PRO_LCD_COLOR_DEPTH_BIT / 8)

#define SMALLTV_PRO_LCD_PX_CLK_HZ (20 * 1000 * 1000)  // TODO: maybe adjus to 80

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

// TODO: determine pixel byte size... 16? 18b?
// "The ST7789 is a single-chip controller/driver for 262K-color, graphic type TFT-LCD. It consists
// of 720 source line and 320 gate line driving circuits. This chip is capable of connecting
// directly to an external microprocessor, and accepts, 8-bits/9-bits/16-bits/18-bits parallel
// interface. Display data can be stored in the on-chip display data RAM of 240x320x18 bits. It can
// perform display data RAM read/write operation with no external operation clock to minimize power
// consumption. In addition, because of the integrated power supply circuit necessary to drive
// liquid crystal; it is possible to make a display system with the fewest components."

#define SMALLTV_PRO_LCD_HOST SPI2_HOST

// TODO
#define SMALLTV_PRO_LVGL_TICK_PERIOD_MS 2

// Since heap since is huge on ESP32, Frame buffer is kept activated by default and no flickering
// effect…

void example_lvgl_demo_ui(lv_display_t *disp) {
    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_t *label = lv_label_create(scr);

    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(label, "Hello Espressif, Hello LVGL. This must be a bit longer to scoll.");

    lv_obj_set_width(label, 240);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
}

// static bool lcd_notify_flush_ready(esp_lcd_panel_io_handle_t panel_io,
// esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
// {
//     lv_display_t *disp = (lv_display_t *)user_ctx;
//     lv_display_flush_ready(disp);
//     return false;
// }

static void lcd_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);

    lv_display_flush_ready(disp);
}

static void lcd_increase_lvgl_tick(void *arg) { lv_tick_inc(SMALLTV_PRO_LVGL_TICK_PERIOD_MS); }

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
        .max_transfer_sz = 80 * SMALLTV_PRO_LCD_H_RES * SMALLTV_PRO_LCD_COLOR_DEPTH_BYTE,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SMALLTV_PRO_LCD_HOST, &buscfg,
                                       SPI_DMA_CH_AUTO));  // Enable the DMA feature

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
    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    const size_t buf_sz =
        SMALLTV_PRO_LCD_H_RES * SMALLTV_PRO_LCD_V_RES * SMALLTV_PRO_LCD_COLOR_DEPTH_BYTE / 10;
    lv_color_t *buf1 = heap_caps_malloc(buf_sz, MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = heap_caps_malloc(buf_sz, MALLOC_CAP_DMA);
    assert(buf2);

    lv_display_t *disp = lv_display_create(SMALLTV_PRO_LCD_H_RES, SMALLTV_PRO_LCD_V_RES);
    lv_display_set_user_data(disp, (void *)panel_handle);
    lv_display_set_flush_cb(disp, lcd_flush_cb);
    lv_display_set_buffers(disp, buf1, buf2, buf_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(disp, SMALLTV_PRO_LCD_COLOR_FORMAT);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    const esp_timer_create_args_t lvgl_tick_timer_args = {.callback = &lcd_increase_lvgl_tick,
                                                          .name = "lvgl_tick"};
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(
        esp_timer_start_periodic(lvgl_tick_timer, SMALLTV_PRO_LVGL_TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Display LVGL animation");
    example_lvgl_demo_ui(disp);

    while (1) {
        // raise the task priority of LVGL and/or reduce the handler period can improve the
        // performance
        vTaskDelay(pdMS_TO_TICKS(10));

        // The task running lv_timer_handler should have lower priority than that running
        // `lv_tick_inc`
        lv_timer_handler();
    }
}
