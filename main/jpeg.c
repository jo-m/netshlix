#include "jpeg.h"

#include <esp_err.h>
#include <esp_log.h>

#include "../managed_components/lvgl__lvgl/src/libs/tjpgd/tjpgd.h"  // Hacky hack - we use lvgl's vendored tjpgd directly.
#include "lcd.h"

static const char *TAG = "jpgdec";

typedef struct jpeg_decode_input_t {
    const uint8_t *data;
    ptrdiff_t data_max_sz;
    ptrdiff_t read_offset;
    esp_lcd_panel_handle_t panel_handle;
} jpeg_decode_input_t;

static size_t jdec_in_func(JDEC *jd, uint8_t *buff, size_t nbyte) {
    jpeg_decode_input_t *u = (jpeg_decode_input_t *)jd->device;

    const ptrdiff_t avail = u->data_max_sz - u->read_offset;
    if (avail <= 0) {
        return 0;
    }

    if (buff == NULL) {
        u->read_offset += nbyte;
        return nbyte;
    }

    const ptrdiff_t sz = (ptrdiff_t)nbyte > avail ? avail : (ptrdiff_t)nbyte;
    assert(sz > 0);
    assert(u->read_offset + sz <= u->data_max_sz);
    memcpy(buff, u->data + u->read_offset, sz);
    u->read_offset += sz;
    return (size_t)sz;
}

int jdec_out_func(JDEC *jd, void *bitmap, JRECT *rect) {
    ESP_LOGD(TAG, "Image block %u %u %u %u", rect->top, rect->bottom, rect->left, rect->right);

    return 1;
}

esp_err_t decode_jpeg(const uint8_t *data, const ptrdiff_t data_max_sz,
                      esp_lcd_panel_handle_t panel_handle) {
    jpeg_decode_input_t u = {0};
    u.data = data;
    u.data_max_sz = data_max_sz;
    u.read_offset = 0;
    u.panel_handle = panel_handle;

    JRESULT res;
    JDEC jdec = {0};
    const size_t work_sz = 3500;
    void *work = malloc(work_sz);
    if (work == NULL) {
        return ESP_ERR_NO_MEM;
    }

    res = jd_prepare(&jdec, jdec_in_func, work, work_sz, (void *)&u);
    if (res != JDR_OK) {
        ESP_LOGE(TAG, "Error: jd_prepare() -> %d", res);
        free(work);
        return ESP_ERR_NOT_FINISHED;
    }
    res = jd_decomp(&jdec, jdec_out_func, 0);
    if (res != JDR_OK) {
        ESP_LOGE(TAG, "Error: jd_prepare() -> %d", res);
        free(work);
        return ESP_ERR_NOT_FINISHED;
    }

    free(work);

    ESP_LOGI(TAG, "Finished decoding");

    return ESP_OK;
}
