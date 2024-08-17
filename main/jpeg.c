#include "jpeg.h"

#include <assert.h>
#include <esp_err.h>
#include <esp_log.h>
#include <string.h>

#include "../managed_components/lvgl__lvgl/src/libs/tjpgd/tjpgd.h"  // Hacky hack - we use lvgl's vendored tjpgd directly.
#include "lcd.h"

#define BLOCK_SZ_PX 16

static const char *TAG = "jpgdec";

typedef struct jpeg_decode_input_t {
    const uint8_t *data;
    ptrdiff_t data_max_sz;
    ptrdiff_t read_offset;
    lcd_t *lcd;

    // Buffer a chunk of display_w_px * block_sz_px of pixels before writing to the display.
    uint16_t *px_buf;
    ptrdiff_t px_buf_sz;
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

_Static_assert(JD_FORMAT == 0);  // RGB888
typedef struct px888_t {
    uint8_t b;
    uint8_t g;
    uint8_t r;
} px888_t;
_Static_assert(sizeof(px888_t) == 3);

static uint16_t rgb565(px888_t px) {
    return ((px.r & 0b11111000) << 8) | ((px.g & 0b11111100) << 3) | (px.b >> 3);
}

// http://elm-chan.org/fsw/tjpgd/en/output.html
static int jdec_out_func(JDEC *jd, void *bitmap, JRECT *rect) {
    ESP_LOGD(TAG, "Image block %hux%hu scl=%hhu t%hu l%hu b%hu r%hu", jd->width, jd->height,
             jd->scale, rect->top, rect->left, rect->bottom, rect->right);

    if (jd->width != SMALLTV_LCD_H_RES || jd->height != SMALLTV_LCD_V_RES || jd->scale != 0 ||
        jd->ncomp != 3 || jd->msx * 8 != BLOCK_SZ_PX || jd->msy * 8 != BLOCK_SZ_PX ||
        (rect->right - rect->left + 1) != BLOCK_SZ_PX ||
        (rect->bottom - rect->top + 1) != BLOCK_SZ_PX) {
        ESP_LOGW(TAG, "Aborting decoding");
        return 0;
    }

    // Copy pixels, converting from RGB888 to RGB565.
    jpeg_decode_input_t *u = (jpeg_decode_input_t *)jd->device;
    assert(u != NULL);
    px888_t *decoded_rgb888 = (px888_t *)bitmap;
    for (int y = 0; y < BLOCK_SZ_PX; y++) {
        for (int x = 0; x < BLOCK_SZ_PX; x++) {
            const px888_t px = decoded_rgb888[y * BLOCK_SZ_PX + x];
            const int ix = y * (SMALLTV_LCD_H_RES) + (x + (int)rect->left);
            u->px_buf[ix] = rgb565(px);
        }
    }

    // Write?
    if (rect->right + 1 == SMALLTV_LCD_H_RES) {
        const int x_start = 0, y_start = rect->top, x_end = SMALLTV_LCD_H_RES - 1,
                  y_end = rect->bottom;
        ESP_LOGD(TAG, "lcd_draw_start() x=[%d %d] y=[%d %d]", x_start, x_end, y_start, y_end);
        lcd_draw_start(u->lcd, x_start, y_start, x_end, y_end, u->px_buf);
        lcd_draw_wait_finished(u->lcd);
    }

    return 1;
}

esp_err_t jpeg_decode_to_lcd(const uint8_t *data, const ptrdiff_t data_max_sz, lcd_t *lcd) {
    jpeg_decode_input_t u = {0};
    u.data = data;
    u.data_max_sz = data_max_sz;
    u.read_offset = 0;
    u.lcd = lcd;
    u.px_buf_sz = BLOCK_SZ_PX * SMALLTV_LCD_H_RES * sizeof(*u.px_buf);
    u.px_buf =
        malloc(u.px_buf_sz);  // TODO: perhaps persist some state to avoid repeating malloc().
    if (u.px_buf == NULL) {
        return ESP_ERR_NO_MEM;
    }

    JRESULT res;
    JDEC jdec = {0};
    const ptrdiff_t work_sz = 3500;
    void *work = malloc(work_sz);
    if (work == NULL) {
        free(u.px_buf);
        return ESP_ERR_NO_MEM;
    }

    res = jd_prepare(&jdec, jdec_in_func, work, work_sz, (void *)&u);
    if (res != JDR_OK) {
        ESP_LOGE(TAG, "Error: jd_prepare() -> %d", res);
        free(u.px_buf);
        free(work);
        return ESP_ERR_NOT_FINISHED;
    }
    res = jd_decomp(&jdec, jdec_out_func, 0);
    if (res != JDR_OK) {
        ESP_LOGE(TAG, "Error: jd_prepare() -> %d", res);
        free(u.px_buf);
        free(work);
        return ESP_ERR_NOT_FINISHED;
    }

    free(u.px_buf);
    free(work);

    ESP_LOGD(TAG, "Finished decoding");

    return ESP_OK;
}
