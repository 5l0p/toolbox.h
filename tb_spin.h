#ifndef TB_SPIN_H
#define TB_SPIN_H

/*
 * tb_spin.h - integer spin box (stepper). The arrows on the right change
 * the value by `step`; Up/Down keys do the same. The value stays within
 * [min, max]. The callback fires on every change. Typing into the box is
 * not supported yet.
 */

#include "tb_core.h"

#define TB_SPIN_AW 16
#define TB_SPIN_PAD 4

typedef struct tb_spin {
    tb_widget base;
    int min, max, value, step;
    tb_action cb;
    void *data;
} tb_spin;

static inline void tb_spin_tri(tb_app *t, int cx, int cy, int up, int c) {
    XPoint p[3];
    if (up) {
        p[0].x = (short)cx;     p[0].y = (short)(cy - 3);
        p[1].x = (short)(cx - 4); p[1].y = (short)(cy + 2);
        p[2].x = (short)(cx + 4); p[2].y = (short)(cy + 2);
    } else {
        p[0].x = (short)cx;     p[0].y = (short)(cy + 3);
        p[1].x = (short)(cx - 4); p[1].y = (short)(cy - 2);
        p[2].x = (short)(cx + 4); p[2].y = (short)(cy - 2);
    }
    tb_poly(t, p, 3, c);
}

static inline void tb_spin_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_spin *s = (tb_spin *)wb;
    tb_fill(t, wb->x, wb->y, wb->w, wb->h, TB_C_BG);
    tb_frame(t, wb->x, wb->y, wb->w, wb->h, TB_C_FG);

    int ax = wb->x + wb->w - TB_SPIN_AW;
    int mid = wb->y + wb->h / 2;
    tb_vline(t, ax, wb->y + 1, wb->y + wb->h - 2, TB_C_FG);
    tb_hline(t, ax, wb->x + wb->w - 1, mid, TB_C_FG);
    tb_spin_tri(t, ax + TB_SPIN_AW / 2, wb->y + wb->h / 4, 1, TB_C_FG);
    tb_spin_tri(t, ax + TB_SPIN_AW / 2, wb->y + 3 * wb->h / 4, 0, TB_C_FG);

    char buf[32];
    snprintf(buf, sizeof buf, "%d", s->value);
    tb_str(t, wb->x + TB_SPIN_PAD, tb_text_base(t, wb->y, wb->h), TB_C_FG, buf);

    if (focused)
        tb_frame(t, wb->x + 2, wb->y + 2, wb->w - 4, wb->h - 4, TB_C_FG);
}

static inline void tb_spin_apply(tb_app *t, tb_spin *s, int v) {
    if (v < s->min) v = s->min;
    if (v > s->max) v = s->max;
    if (v == s->value) return;
    s->value = v;
    if (s->cb) s->cb(t, s->data);
}

static inline int tb_spin_press(tb_app *t, tb_widget *wb, int x, int y) {
    tb_spin *s = (tb_spin *)wb;
    int ax = wb->x + wb->w - TB_SPIN_AW;
    if (x < ax) return 0;
    int st = s->step > 0 ? s->step : 1;
    if (y < wb->y + wb->h / 2) tb_spin_apply(t, s, s->value + st);
    else                       tb_spin_apply(t, s, s->value - st);
    return 0;
}

static inline void tb_spin_key(tb_app *t, tb_widget *wb, XKeyEvent *e) {
    tb_spin *s = (tb_spin *)wb;
    KeySym ks = XLookupKeysym(e, 0);
    int st = s->step > 0 ? s->step : 1;
    if (ks == XK_Up || ks == XK_Right)        tb_spin_apply(t, s, s->value + st);
    else if (ks == XK_Down || ks == XK_Left)  tb_spin_apply(t, s, s->value - st);
}

static inline void tb_spin_wheel(tb_app *t, tb_widget *wb, int dir) {
    tb_spin *s = (tb_spin *)wb;
    int st = s->step > 0 ? s->step : 1;
    tb_spin_apply(t, s, s->value - dir * st);
}

static inline void tb_spin_destroy(tb_widget *wb) {
    free(wb);
}

static const tb_wops TB_SPIN_OPS = {
    tb_spin_draw, tb_spin_press, NULL, NULL,
    tb_spin_wheel, tb_spin_key, tb_spin_destroy, NULL
};

static inline tb_spin *tb_spin_new(tb_app *t, int x, int y, int w, int h,
                                   int min, int max, int value, int step,
                                   tb_action cb, void *data) {
    tb_spin *s = calloc(1, sizeof *s);
    if (!s) tb_oom();
    s->base.ops = &TB_SPIN_OPS;
    s->base.x = x;
    s->base.y = y;
    s->base.w = w;
    s->base.h = h;
    s->base.clickable = 1;
    s->base.focusable = 1;
    s->min = min;
    s->max = max < min ? min : max;
    s->step = step > 0 ? step : 1;
    s->value = value;
    if (s->value < s->min) s->value = s->min;
    if (s->value > s->max) s->value = s->max;
    s->cb = cb;
    s->data = data;
    tb_add_widget(t, &s->base);
    return s;
}

static inline void tb_spin_set(tb_spin *s, int value) {
    if (value < s->min) value = s->min;
    if (value > s->max) value = s->max;
    s->value = value;
    if (s->base.app) tb_damage(s->base.app, &s->base);
}

static inline int tb_spin_get(const tb_spin *s) {
    return s->value;
}

#endif /* TB_SPIN_H */
