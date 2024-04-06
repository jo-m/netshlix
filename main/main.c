#include <stdio.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "example";

void app_main(void) {
    while (1) {
        ESP_LOGI(TAG, "Hello %d!", 42);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
