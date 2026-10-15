#ifndef TB_SLIDER_H
#define TB_SLIDER_H

/*
 * tb_slider.h - horizontal or vertical slider with a knob. Dragging,
 * clicking the track and the arrow keys change the value; the callback
 * fires on every change. The value is snapped to `step`.
 */

#include "tb_core.h"

#define TB_SLIDER_KNOB 11

typedef struct tb_slider {
    tb_widget base;
    int vertical;
    int min, max, value, step;
    int dragging;
    tb_action cb;
    void *data;
} tb_slider;

/* Knob centre positions along the slider: [lo, hi]. */
static inline void tb_slider_span(const tb_slider *s, int *lo, int *hi) {
    int len = s->vertical ? s->base.h : s->base.w;
    *lo = TB_SLIDER_KNOB / 2;
    *hi = len - TB_SLIDER_KNOB / 2 - 1;
    if (*hi < *lo) *hi = *lo;
}

static inline int tb_slider_clamp(const tb_slider *s, int v) {
    if (v < s->min) v = s->min;
    if (v > s->max) v = s->max;
    return v;
}

static inline void tb_slider_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_slider *s = (tb_slider *)wb;
    int lo, hi;
    tb_slider_span(s, &lo, &hi);
    int range = s->max - s->min;
    int pos = lo;
    if (range > 0) pos = lo + (int)((long)(s->value - s->min) * (hi - lo) / range);
    int k = TB_SLIDER_KNOB;
    int kc = s->dragging ? TB_C_FG : TB_C_BG;

    if (!s->vertical) {
        int gy = wb->y + wb->h / 2 - 1;
        tb_fill(t, wb->x + k / 2, gy, wb->w - k, 3, TB_C_BG);
        tb_frame(t, wb->x + k / 2, gy, wb->w - k, 3, TB_C_FG);
        int kx = wb->x + pos - k / 2, ky = wb->y + (wb->h - k) / 2;
        tb_fill(t, kx, ky, k, k, kc);
        tb_frame(t, kx, ky, k, k, TB_C_FG);
    } else {
        int gx = wb->x + wb->w / 2 - 1;
        tb_fill(t, gx, wb->y + k / 2, 3, wb->h - k, TB_C_BG);
        tb_frame(t, gx, wb->y + k / 2, 3, wb->h - k, TB_C_FG);
        int ky = wb->y + pos - k / 2, kx = wb->x + (wb->w - k) / 2;
        tb_fill(t, kx, ky, k, k, kc);
        tb_frame(t, kx, ky, k, k, TB_C_FG);
    }
    if (focused)
        tb_frame(t, wb->x, wb->y, wb->w, wb->h, TB_C_FG);
}

/* Set the value, snapping to the step, and fire the callback on change. */
static inline void tb_slider_apply(tb_app *t, tb_slider *s, int v) {
    if (s->step > 1) v = s->min + ((v - s->min + s->step / 2) / s->step) * s->step;
    v = tb_slider_clamp(s, v);
    if (v == s->value) return;
    s->value = v;
    if (s->cb) s->cb(t, s->data);
}

/* Value under the pointer (for clicks and drags). */
static inline void tb_slider_from_pointer(tb_app *t, tb_slider *s, int x, int y) {
    int lo, hi;
    tb_slider_span(s, &lo, &hi);
    int along = s->vertical ? y - s->base.y : x - s->base.x;
    int range = s->max - s->min;
    int v = s->min;
    if (hi > lo && range > 0) {
        int a = along - lo;
        if (a < 0) a = 0;
        if (a > hi - lo) a = hi - lo;
        v = s->min + (int)((long)a * range / (hi - lo) + 0);
    }
    tb_slider_apply(t, s, v);
}

static inline int tb_slider_press(tb_app *t, tb_widget *wb, int x, int y) {
    tb_slider *s = (tb_slider *)wb;
    s->dragging = 1;
    tb_slider_from_pointer(t, s, x, y);
    return 1;
}

static inline void tb_slider_motion(tb_app *t, tb_widget *wb, int x, int y) {
    tb_slider *s = (tb_slider *)wb;
    if (s->dragging) tb_slider_from_pointer(t, s, x, y);
}

static inline void tb_slider_release(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t; (void)x; (void)y;
    ((tb_slider *)wb)->dragging = 0;
}

static inline void tb_slider_key(tb_app *t, tb_widget *wb, XKeyEvent *e) {
    tb_slider *s = (tb_slider *)wb;
    KeySym ks = XLookupKeysym(e, 0);
    int st = s->step > 0 ? s->step : 1;
    if (ks == XK_Left || ks == XK_Down)       tb_slider_apply(t, s, s->value - st);
    else if (ks == XK_Right || ks == XK_Up)   tb_slider_apply(t, s, s->value + st);
}

static inline void tb_slider_wheel(tb_app *t, tb_widget *wb, int dir) {
    tb_slider *s = (tb_slider *)wb;
    int st = s->step > 0 ? s->step : 1;
    tb_slider_apply(t, s, s->value - dir * st);
}

static inline void tb_slider_destroy(tb_widget *wb) {
    free(wb);
}

static const tb_wops TB_SLIDER_OPS = {
    tb_slider_draw, tb_slider_press, tb_slider_motion, tb_slider_release,
    tb_slider_wheel, tb_slider_key, tb_slider_destroy, NULL
};

static inline tb_slider *tb_slider_new(tb_app *t, int x, int y, int w, int h,
                                       int vertical, int min, int max, int value,
                                       tb_action cb, void *data) {
    tb_slider *s = calloc(1, sizeof *s);
    if (!s) tb_oom();
    s->base.ops = &TB_SLIDER_OPS;
    s->base.x = x;
    s->base.y = y;
    s->base.w = w;
    s->base.h = h;
    s->base.clickable = 1;
    s->base.focusable = 1;
    s->vertical = vertical ? 1 : 0;
    s->min = min;
    s->max = max < min ? min : max;
    s->step = 1;
    s->value = tb_slider_clamp(s, value);
    s->cb = cb;
    s->data = data;
    tb_add_widget(t, &s->base);
    return s;
}

/* Set the value without firing the callback. */
static inline void tb_slider_set(tb_slider *s, int value) {
    s->value = tb_slider_clamp(s, value);
    if (s->base.app) tb_damage(s->base.app, &s->base);
}

static inline int tb_slider_get(const tb_slider *s) {
    return s->value;
}

#endif /* TB_SLIDER_H */
