#ifndef TB_SPLIT_H
#define TB_SPLIT_H

/*
 * tb_split.h - horizontal splitter bar. Drag it up or down to resize the
 * panes above and below it. The widget's y is the split position; the app
 * lays out its panes from split->base.y and gets a callback whenever the
 * bar moves. min_top and min_bottom are the smallest y and the smallest
 * distance from the bottom of the window that the bar may reach.
 */

#include "tb_core.h"

#define TB_SPLIT_BAR 6

typedef struct tb_split {
    tb_widget base;
    int min_top, min_bottom;
    int drag_off;
    tb_action cb;
    void *data;
} tb_split;

static inline void tb_split_draw(tb_app *t, tb_widget *wb, int focused) {
    (void)focused;
    tb_fill(t, wb->x, wb->y, wb->w, wb->h, TB_C_BG);
    tb_frame(t, wb->x, wb->y, wb->w, wb->h, TB_C_FG);
    /* grip in the middle */
    int mid = wb->y + wb->h / 2, cx = wb->x + wb->w / 2;
    tb_hline(t, cx - 12, cx + 12, mid - 1, TB_C_FG);
    tb_hline(t, cx - 12, cx + 12, mid + 1, TB_C_FG);
}

static inline int tb_split_press(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t; (void)x;
    tb_split *s = (tb_split *)wb;
    s->drag_off = y - wb->y;
    return 1;                               /* grab: motion comes to us */
}

static inline void tb_split_motion(tb_app *t, tb_widget *wb, int x, int y) {
    (void)x;
    tb_split *s = (tb_split *)wb;
    int lo = s->min_top;
    int hi = t->height - wb->h - s->min_bottom;
    if (hi < lo) hi = lo;
    int ny = y - s->drag_off;
    if (ny < lo) ny = lo;
    if (ny > hi) ny = hi;
    if (ny == wb->y) return;
    wb->y = ny;
    if (s->cb) s->cb(t, s->data);
}

static inline void tb_split_destroy(tb_widget *wb) {
    free(wb);
}

static const tb_wops TB_SPLIT_OPS = {
    tb_split_draw, tb_split_press, tb_split_motion, NULL,
    NULL, NULL, tb_split_destroy, NULL
};

/* Position the bar at y. Call tb_split_set() to move it from code. */
static inline tb_split *tb_split_new(tb_app *t, int x, int y, int w,
                                     int min_top, int min_bottom,
                                     tb_action cb, void *data) {
    tb_split *s = calloc(1, sizeof *s);
    if (!s) tb_oom();
    s->base.ops = &TB_SPLIT_OPS;
    s->base.x = x;
    s->base.y = y;
    s->base.w = w;
    s->base.h = TB_SPLIT_BAR;
    s->base.clickable = 1;
    s->min_top = min_top;
    s->min_bottom = min_bottom;
    s->cb = cb;
    s->data = data;
    tb_add_widget(t, &s->base);
    return s;
}

#endif /* TB_SPLIT_H */
