#ifndef TB_BUTTON_H
#define TB_BUTTON_H

/*
 * tb_button.h - push button. Inverts while pressed; the callback fires
 * on release if the pointer is still over the button.
 */

#include "tb_core.h"

typedef struct tb_button {
    tb_widget base;
    char *text;
    tb_action cb;
    void *data;
    int pressed, inside;
} tb_button;

static inline void tb_button_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_button *b = (tb_button *)wb;
    (void)focused;
    int inv = b->pressed && b->inside;
    int bg = inv ? TB_C_FG : TB_C_BG;
    int fg = inv ? TB_C_BG : TB_C_FG;
    tb_fill(t, wb->x, wb->y, wb->w, wb->h, bg);
    for (int i = 0; i < TB_BUTTON_BORDER; i++)
        tb_frame(t, wb->x + i, wb->y + i, wb->w - 2 * i, wb->h - 2 * i, TB_C_FG);
    int tw = tb_text_w(t, b->text);
    tb_str(t, wb->x + (wb->w - tw) / 2, tb_text_base(t, wb->y, wb->h), fg, b->text);
}

static inline int tb_button_press(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t; (void)x; (void)y;
    tb_button *b = (tb_button *)wb;
    b->pressed = 1;
    b->inside = 1;
    return 1;
}

static inline void tb_button_motion(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t;
    tb_button *b = (tb_button *)wb;
    b->inside = tb_inside(wb, x, y);
}

static inline void tb_button_release(tb_app *t, tb_widget *wb, int x, int y) {
    (void)x; (void)y;
    tb_button *b = (tb_button *)wb;
    int fire = b->inside;
    b->pressed = 0;
    b->inside = 0;
    if (fire && b->cb) b->cb(t, b->data);
}

static inline void tb_button_destroy(tb_widget *wb) {
    tb_button *b = (tb_button *)wb;
    free(b->text);
    free(b);
}

static const tb_wops TB_BUTTON_OPS = {
    tb_button_draw, tb_button_press, tb_button_motion, tb_button_release,
    NULL, NULL, tb_button_destroy
};

static inline tb_button *tb_button_new(tb_app *t, int x, int y, int w, int h,
                                       const char *label, tb_action cb, void *data) {
    tb_button *b = calloc(1, sizeof *b);
    if (!b) tb_oom();
    b->base.ops = &TB_BUTTON_OPS;
    b->base.x = x;
    b->base.y = y;
    b->base.w = w;
    b->base.h = h;
    b->base.clickable = 1;
    b->text = tb_strdup(label);
    b->cb = cb;
    b->data = data;
    tb_add_widget(t, &b->base);
    return b;
}

#endif /* TB_BUTTON_H */
