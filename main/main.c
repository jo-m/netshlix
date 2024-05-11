#include <esp_err.h>
#include <esp_log.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <freertos/FreeRTOS.h>
#pragma GCC diagnostic pop
#include <freertos/task.h>
#include <math.h>
#include <nvs_flash.h>
#include <stdio.h>

#include "display.h"
#include "dns.h"
#include "jpeg.h"
#include "lcd.h"
#include "lvgl.h"
#include "rtp_jpeg.h"
#include "rtp_udp.h"
#include "sdkconfig.h"
#include "smpte_bars.h"
#include "wifi.h"

static const char *TAG = "main";

static void print_free_heap_stack() {
    ESP_LOGI(TAG, "=== Free: 8BIT=%u largest_block=%u heap=%lu stack=%d",
             heap_caps_get_free_size(MALLOC_CAP_8BIT),
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), esp_get_free_heap_size(),
             uxTaskGetStackHighWaterMark(NULL));
}

// Expects a QueueHandle_t<uint8_t[CONFIG_RTP_JPEG_MAX_DATA_SIZE_BYTES]> as pvParameters argument.
void decode_jpeg_task(void *pvParameters) {
    QueueHandle_t in = (QueueHandle_t)pvParameters;

    // Our copy of the frame for the decoder to read from.
    uint8_t buf[CONFIG_RTP_JPEG_MAX_DATA_SIZE_BYTES] = {0};

    while (true) {
        memset(buf, 0, sizeof(buf));
        if (!xQueueReceive(in, &buf, 0)) {
            ESP_LOGD(TAG, "Received nothing");
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        ESP_LOGD(TAG, "Received frame from queue");

        // Now, decode.
        ESP_ERROR_CHECK(decode_jpeg(buf, sizeof(buf), NULL));  // TODO: panel_handle
    }
}

size_t decode_jpeg_task_approx_stack_sz() { return CONFIG_RTP_JPEG_MAX_DATA_SIZE_BYTES + 100; }

void app_main(void) {
    ESP_LOGI(TAG, "app_main()");

    ESP_LOGI(TAG, "Initialize NVS");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Initialize WIFI");
    wifi_init();

    ESP_LOGI(TAG, "Initialize mDNS");
    mdns_svr_init();

    ESP_LOGI(TAG, "Initialize LCD");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_io_handle_t panel_io_handle = NULL;
    ESP_ERROR_CHECK(lcd_init(&panel_handle, &panel_io_handle));
    assert(panel_handle != NULL);
    print_free_heap_stack();

    ESP_LOGI(TAG, "Initialize display");
    lv_display_t *disp = NULL;
    ESP_ERROR_CHECK(display_init(panel_handle, panel_io_handle, &disp));
    assert(disp != NULL);
    print_free_heap_stack();

    ESP_LOGI(TAG, "Display SMPTE test image");
    lv_obj_t *scr = lv_display_get_screen_active(disp);
    make_smpte_image(scr);
    print_free_heap_stack();

    ESP_LOGI(TAG, "Initializing JPEG receive buffer");
    QueueHandle_t rtp_out = xQueueCreate(1, CONFIG_RTP_JPEG_MAX_DATA_SIZE_BYTES);
    assert(rtp_out != NULL);
    print_free_heap_stack();

    ESP_LOGI(TAG, "Starting UDP server");
    ESP_LOGI(TAG, "Starting task, stack_sz=%u", rtp_udp_recv_task_approx_stack_sz());
    const BaseType_t err0 =
        xTaskCreate(rtp_udp_recv_task, "rtp_udp_recv_task", rtp_udp_recv_task_approx_stack_sz(),
                    (void *)rtp_out, 5, NULL);
    if (err0 != pdPASS) {
        ESP_LOGE(TAG, "Failed to start task: %d", err0);
        abort();
    }
    print_free_heap_stack();

    ESP_LOGI(TAG, "Starting JPEG task");
    ESP_LOGI(TAG, "Starting task, stack_sz=%u", decode_jpeg_task_approx_stack_sz());
    const BaseType_t err1 =
        xTaskCreate(decode_jpeg_task, "decode_jpeg_task", decode_jpeg_task_approx_stack_sz(),
                    (void *)rtp_out, 5, NULL);
    if (err1 != pdPASS) {
        ESP_LOGE(TAG, "Failed to start task: %d", err1);
        abort();
    }
    print_free_heap_stack();

    while (1) {
        uint32_t time_till_next_ms = lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(time_till_next_ms));
    }
}
