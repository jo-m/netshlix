#include "jpeg.h"

#include <assert.h>
#include <esp_err.h>
#include <esp_log.h>

#include "../managed_components/lvgl__lvgl/src/libs/tjpgd/tjpgd.h"  // Hacky hack - we use lvgl's vendored tjpgd directly.
#include "lcd.h"

static const char *TAG = "jpgdec";

typedef struct jpeg_decode_input_t {
    const uint8_t *data;
    ptrdiff_t data_max_sz;
    ptrdiff_t read_offset;
    lcd_t *lcd;
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

static uint16_t rgb565_888(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | (b >> 3);
}

typedef struct px888_t {
    uint8_t b;
    uint8_t g;
    uint8_t r;
} px888_t;
_Static_assert(sizeof(px888_t) == 3);

static uint16_t buf_RGB565[16 * 16] = {0};

// http://elm-chan.org/fsw/tjpgd/en/output.html
static int jdec_out_func(JDEC *jd, void *bitmap, JRECT *rect) {
    ESP_LOGD(TAG, "Image block %hux%hu scl=%hhu t%hu l%hu b%hu r%hu", jd->width, jd->height,
             jd->scale, rect->top, rect->left, rect->bottom, rect->right);

    if (jd->width != SMALLTV_LCD_H_RES || jd->height != SMALLTV_LCD_V_RES || jd->scale != 0 ||
        jd->ncomp != 3 || jd->msx != 2 || jd->msy != 2) {
        // Abort decoding.
        ESP_LOGW(TAG, "Aborting decoding");
        return 0;
    }

    jpeg_decode_input_t *u = (jpeg_decode_input_t *)jd->device;
    assert(u != NULL);

    const int rect_w_px = rect->right - rect->left + 1, rect_h_px = rect->bottom - rect->top + 1;
    assert(rect_w_px == 16);
    assert(rect_h_px == 16);

    // TODO: we actually want RGB565, configure JD_FORMAT accordingly
    _Static_assert(JD_FORMAT == 0);  // RGB888
    px888_t *buf_RGB888 = (px888_t *)bitmap;
    memset(buf_RGB565, 0, sizeof(buf_RGB565));
    for (int y = 0; y < rect_h_px; y++) {
        for (int x = 0; x < rect_w_px; x++) {
            px888_t px = buf_RGB888[y * rect_h_px + x];
            buf_RGB565[y * rect_h_px + x] = rgb565_888(px.r, px.g, px.b);
        }
    }

    lcd_draw_start(u->lcd, rect->left, rect->top, rect->right, rect->bottom, buf_RGB565);
    lcd_draw_wait_finished(u->lcd);

    return 1;
}

esp_err_t jpeg_decode_to_lcd(const uint8_t *data, const ptrdiff_t data_max_sz, lcd_t *lcd) {
    jpeg_decode_input_t u = {0};
    u.data = data;
    u.data_max_sz = data_max_sz;
    u.read_offset = 0;
    u.lcd = lcd;

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
