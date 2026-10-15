#ifndef TB_PROGRESS_H
#define TB_PROGRESS_H

/*
 * tb_progress.h - determinate progress bar. Value runs from 0 to max.
 * Indeterminate mode needs timers, which the toolkit does not have yet.
 */

#include "tb_core.h"

typedef struct tb_progress {
    tb_widget base;
    int max, value;
} tb_progress;

static inline void tb_progress_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_progress *p = (tb_progress *)wb;
    (void)focused;
    tb_fill(t, wb->x, wb->y, wb->w, wb->h, TB_C_BG);
    tb_frame(t, wb->x, wb->y, wb->w, wb->h, TB_C_FG);
    int iw = wb->w - 6, ih = wb->h - 6;
    if (iw < 1 || ih < 1 || p->max <= 0) return;
    int fw = (int)((long)iw * p->value / p->max);
    if (fw > 0) tb_fill(t, wb->x + 3, wb->y + 3, fw, ih, TB_C_FG);
}

static inline void tb_progress_destroy(tb_widget *wb) {
    free(wb);
}

static const tb_wops TB_PROGRESS_OPS = {
    tb_progress_draw, NULL, NULL, NULL, NULL, NULL, tb_progress_destroy, NULL
};

static inline tb_progress *tb_progress_new(tb_app *t, int x, int y, int w, int h, int max) {
    tb_progress *p = calloc(1, sizeof *p);
    if (!p) tb_oom();
    p->base.ops = &TB_PROGRESS_OPS;
    p->base.x = x;
    p->base.y = y;
    p->base.w = w;
    p->base.h = h;
    p->max = max > 0 ? max : 1;
    tb_add_widget(t, &p->base);
    return p;
}

static inline void tb_progress_set(tb_progress *p, int value) {
    if (value < 0) value = 0;
    if (value > p->max) value = p->max;
    p->value = value;
    if (p->base.app) tb_damage(p->base.app, &p->base);
}

#endif /* TB_PROGRESS_H */
