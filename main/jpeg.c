#include "jpeg.h"

#include <assert.h>
#include <esp_err.h>
#include <esp_log.h>
#include <stddef.h>
#include <string.h>

#include "../managed_components/lvgl__lvgl/src/libs/tjpgd/tjpgd.h"  // Hacky hack - we use lvgl's vendored tjpgd directly.
#include "lcd.h"

#define BLOCK_SZ_PX 16

static const char *TAG = "jpgdec";

const ptrdiff_t TJPGD_WORK_SZ = 3584;

static size_t jdec_in_func(JDEC *jd, uint8_t *buff, size_t nbyte) {
    jpeg_decoder_t *d = (jpeg_decoder_t *)jd->device;
    assert(d != NULL);

    const ptrdiff_t avail = d->data_max_sz - d->read_offset;
    if (avail <= 0) {
        return 0;
    }

    if (buff == NULL) {
        d->read_offset += nbyte;
        return nbyte;
    }

    const ptrdiff_t sz = (ptrdiff_t)nbyte > avail ? avail : (ptrdiff_t)nbyte;
    assert(sz > 0);
    assert(d->read_offset + sz <= d->data_max_sz);
    memcpy(buff, d->data + d->read_offset, sz);
    d->read_offset += sz;
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
    jpeg_decoder_t *u = (jpeg_decoder_t *)jd->device;
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

esp_err_t init_jpeg_decoder(const ptrdiff_t data_max_sz, lcd_t *lcd, jpeg_decoder_t *out) {
    assert(lcd != NULL);
    assert(out != NULL);
    assert(data_max_sz > 0);
    memset(out, 0, sizeof(*out));

    out->data_max_sz = data_max_sz;
    out->read_offset = 0;
    out->lcd = lcd;

    out->px_buf_sz = BLOCK_SZ_PX * SMALLTV_LCD_H_RES * sizeof(*out->px_buf);
    out->px_buf = malloc(out->px_buf_sz);
    if (out->px_buf == NULL) {
        ESP_LOGW(TAG, "failed alloc of px_buf sz=%d", out->px_buf_sz);
        return ESP_ERR_NO_MEM;
    }

    out->jdec = malloc(sizeof(*(out->jdec)));
    if (out->jdec == NULL) {
        ESP_LOGW(TAG, "failed alloc of js sz=%u", sizeof(*(out->jdec)));
        free(out->px_buf);
        return ESP_ERR_NO_MEM;
    }

    out->work = malloc(TJPGD_WORK_SZ);
    if (out->work == NULL) {
        ESP_LOGW(TAG, "failed alloc of work arena sz=%d", TJPGD_WORK_SZ);
        free(out->px_buf);
        free(out->jdec);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t jpeg_decoder_decode_to_lcd(jpeg_decoder_t *d, const uint8_t *data) {
    assert(d != NULL);
    assert(data != NULL);

    assert(d->data_max_sz > 0);
    d->data = data;
    d->read_offset = 0;

    memset(d->px_buf, 0, d->px_buf_sz);
    memset(d->work, 0, TJPGD_WORK_SZ);

    JRESULT res = jd_prepare(d->jdec, jdec_in_func, d->work, TJPGD_WORK_SZ, (void *)d);
    if (res != JDR_OK) {
        ESP_LOGE(TAG, "Error: jd_prepare() -> %d", res);
        return ESP_ERR_NOT_FINISHED;
    }

    res = jd_decomp(d->jdec, jdec_out_func, 0);
    if (res != JDR_OK) {
        ESP_LOGE(TAG, "Error: jd_decomp() -> %d", res);
        return ESP_ERR_NOT_FINISHED;
    }

    ESP_LOGD(TAG, "Finished decoding");

    return ESP_OK;
}

void jpeg_decoder_destroy(jpeg_decoder_t *d) {
    assert(d != NULL);
    free(d->px_buf);
    free(d->work);
    memset(d, 0, sizeof(*d));
}
