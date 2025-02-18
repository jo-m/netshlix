#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <freertos/FreeRTOS.h>
#pragma GCC diagnostic pop
#include <freertos/task.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <string.h>

#include "dns.h"
#include "jpeg.h"
#include "lcd.h"
#include "lvgl_display.h"
#include "rtp_udp.h"
#include "sdkconfig.h"
#include "smpte_bars.h"
#include "wifi.h"

static const char *TAG = "main";

static const int64_t FRAME_TIMEOUT_US = 500 * 1000;

static void print_free_heap_stack() {
    ESP_LOGI(TAG, "=== Free: 8BIT=%u largest_block=%u heap=%lu stack=%d",
             heap_caps_get_free_size(MALLOC_CAP_8BIT),
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), esp_get_free_heap_size(),
             uxTaskGetStackHighWaterMark(NULL));
}

// Our copy of the frame for the decoder to read from.
static uint8_t decode_in_buf[CONFIG_RTP_JPEG_MAX_DATA_SIZE_BYTES] = {0};

void app_main(void) {
    ESP_LOGI(TAG, "app_main()");

    ESP_LOGI(TAG, "Initialize NVS");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    print_free_heap_stack();
    ESP_LOGI(TAG, "Initialize LCD");
    lcd_t lcd = {0};
    init_lcd(&lcd);

    print_free_heap_stack();
    ESP_LOGI(TAG, "Initialize display");
    lv_display_t *disp = NULL;
    init_lvgl_display(&lcd, &disp);
    assert(disp != NULL);

    print_free_heap_stack();
    ESP_LOGI(TAG, "Display SMPTE test image");
    lv_obj_t *scr = lv_display_get_screen_active(disp);
    init_smpte_image(scr);
    smpte_image_set_text("Initializing...");
    lv_timer_handler();

    print_free_heap_stack();
    ESP_LOGI(TAG, "Initialize WIFI");
    init_wifi();

    print_free_heap_stack();
    ESP_LOGI(TAG, "Initialize mDNS");
    init_mdns_svr();

    print_free_heap_stack();
    ESP_LOGI(TAG, "Initializing JPEG receive buffer");
    QueueHandle_t rtp_out = xQueueCreate(1, CONFIG_RTP_JPEG_MAX_DATA_SIZE_BYTES);
    assert(rtp_out != NULL);

    print_free_heap_stack();
    ESP_LOGI(TAG, "Starting UDP server task, stack_sz=%u", rtp_udp_recv_task_approx_stack_sz());
    const BaseType_t err0 =
        xTaskCreate(rtp_udp_recv_task, "rtp_udp_recv_task", rtp_udp_recv_task_approx_stack_sz(),
                    (void *)rtp_out, 5, NULL);
    if (err0 != pdPASS) {
        ESP_LOGE(TAG, "Failed to start task: %d", err0);
        abort();
    }

    print_free_heap_stack();
    ESP_LOGI(TAG, "Initializing JPEG decoder");
    jpeg_decoder_t jpeg_dec = {0};
    ESP_ERROR_CHECK(init_jpeg_decoder(&lcd, &jpeg_dec));

    // Main loop.
    print_free_heap_stack();
    smpte_image_set_text("No stream available       ");
    int64_t last_frame_recv_us = 0;
    bool reset_screen = false;
    while (1) {
        // If last frame was received too long ago, show test image via LVGL.
        const int64_t now_us = esp_timer_get_time();
        const int64_t last_frame_ago_us = now_us - last_frame_recv_us;
        if (last_frame_ago_us > FRAME_TIMEOUT_US) {
            if (reset_screen) {
                lv_obj_invalidate(scr);
                reset_screen = false;
            }
            uint32_t time_till_next_ms = lv_timer_handler();
            vTaskDelay(pdMS_TO_TICKS(time_till_next_ms));
        }

        // Wait some ticks for a frame, continue if none.
        if (!xQueueReceive(rtp_out, &decode_in_buf, pdMS_TO_TICKS(10))) {
            ESP_LOGD(TAG, "Received nothing");
            continue;
        }

        // Display frame and update `last_frame_recv_us`.
        ESP_LOGI(TAG, "Received frame, decode");
        last_frame_recv_us = esp_timer_get_time();
        reset_screen = true;
        const esp_err_t err =
            jpeg_decoder_decode_to_lcd(&jpeg_dec, decode_in_buf, sizeof(decode_in_buf));
        if (err == ESP_OK) {
            const int64_t t1 = esp_timer_get_time();
            ESP_LOGI(TAG, "Decoded frame dt=%lldus", t1 - last_frame_recv_us);
        } else {
            ESP_LOGW(TAG, "Decoding frame failed: %s (%d)", esp_err_to_name(err), err);
        }
    }
}
