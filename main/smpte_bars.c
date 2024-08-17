#include "smpte_bars.h"

#include <assert.h>
#include <esp_heap_caps.h>

#include "lcd.h"

static lv_obj_t *init_rect(lv_obj_t *scr, int32_t w, int32_t h, int32_t x, int32_t y,
                           lv_color_t color) {
    lv_obj_t *rect = lv_obj_create(scr);
    lv_obj_set_size(rect, w, h);
    lv_obj_set_pos(rect, x, y);
    lv_obj_set_style_border_width(rect, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(rect, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(rect, color, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(rect, LV_SCROLLBAR_MODE_OFF);
    return rect;
}

static const int canvas_w = 60, canvas_h = 60;
static const int canvas_buf_sz = canvas_w * canvas_h * sizeof(uint16_t);

static uint32_t rand32() {
    static uint32_t x = 123;
    x ^= (x << 13);
    x ^= (x >> 17);
    return x ^= (x << 5);
}

static uint16_t rgb565_888(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | (b >> 3);
}

static void anim_cb(void *var, int32_t v __attribute__((unused))) {
    assert(var != NULL);
    lv_obj_t *canvas = (lv_obj_t *)var;
    lv_draw_buf_t *buf = lv_canvas_get_draw_buf(canvas);
    assert(buf != NULL);
    assert(buf->data_size == canvas_buf_sz);
    uint16_t *canvas_buf = buf->unaligned_data;
    assert(canvas_buf != NULL);

    for (int i = 0; i < canvas_w * canvas_h; i++) {
        const uint8_t val = (uint8_t)(rand32() % 255);
        canvas_buf[i] = rgb565_888(val, val, val);
    }
    lv_obj_invalidate(canvas);
}

void init_smpte_image(lv_obj_t *scr) {
    _Static_assert(SMALLTV_LCD_H_RES == 240);
    _Static_assert(SMALLTV_LCD_V_RES == 240);

    init_rect(scr, 34, 160, 34 * 0, 0, lv_color_hex(0xffffff));
    init_rect(scr, 34, 160, 34 * 1, 0, lv_color_hex(0xffff00));
    init_rect(scr, 34, 160, 34 * 2, 0, lv_color_hex(0x00ffff));
    init_rect(scr, 34, 160, 34 * 3, 0, lv_color_hex(0x00ff00));
    init_rect(scr, 34, 160, 34 * 4, 0, lv_color_hex(0xff00ff));
    init_rect(scr, 34, 160, 34 * 5, 0, lv_color_hex(0xff0000));
    init_rect(scr, 36, 160, 34 * 6, 0, lv_color_hex(0x0000ff));

    init_rect(scr, 34, 20, 34 * 0, 160, lv_color_hex(0x0000ff));
    init_rect(scr, 34, 20, 34 * 1, 160, lv_color_hex(0x000000));
    init_rect(scr, 34, 20, 34 * 2, 160, lv_color_hex(0xff00ff));
    init_rect(scr, 34, 20, 34 * 3, 160, lv_color_hex(0x000000));
    init_rect(scr, 34, 20, 34 * 4, 160, lv_color_hex(0x00ffff));
    init_rect(scr, 34, 20, 34 * 5, 160, lv_color_hex(0x000000));
    init_rect(scr, 36, 20, 34 * 6, 160, lv_color_hex(0xffffff));

    init_rect(scr, 40, 60, 40 * 0, 180, lv_color_hex(0x000080));
    init_rect(scr, 40, 60, 40 * 1, 180, lv_color_hex(0xffffff));
    init_rect(scr, 40, 60, 40 * 2, 180, lv_color_hex(0x0080ff));
    init_rect(scr, 40, 60, 40 * 3, 180, lv_color_hex(0x000000));
    init_rect(scr, 20, 60, 40 * 4, 180, lv_color_hex(0x131313));

    // Static noise canvas.
    uint16_t *canvas_buf = heap_caps_malloc(canvas_buf_sz, MALLOC_CAP_8BIT);
    assert(canvas_buf != NULL);
    lv_obj_t *canvas = lv_canvas_create(scr);
    lv_canvas_set_buffer(canvas, canvas_buf, 60, 60, LV_COLOR_FORMAT_RGB565);
    lv_canvas_fill_bg(canvas, lv_color_hex(0x000000), LV_OPA_COVER);
    lv_obj_set_pos(canvas, 180, 180);

    // Animate static noise.
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, canvas);
    lv_anim_set_duration(&a, 1000);
    lv_anim_set_values(&a, 1, 10);
    lv_anim_set_exec_cb(&a, anim_cb);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}
