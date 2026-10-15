#ifndef TB_COLORGRID_H
#define TB_COLORGRID_H

/*
 * tb_colorgrid.h - the 16 colours from config.h as an 8 x 2 grid of
 * swatches. Click a swatch to select it; the callback fires on change.
 * (Named colorgrid because `tb_palette` is the colour table in config.h.)
 */

#include "tb_core.h"

#define TB_GRID_COLS 8
#define TB_GRID_ROWS 2

typedef struct tb_colorgrid {
    tb_widget base;
    int sel;
    tb_action cb;
    void *data;
} tb_colorgrid;

static inline void tb_colorgrid_cell(const tb_widget *w, int i, int *cx, int *cy, int *cw, int *ch) {
    *cw = w->w / TB_GRID_COLS;
    *ch = w->h / TB_GRID_ROWS;
    *cx = w->x + (i % TB_GRID_COLS) * *cw;
    *cy = w->y + (i / TB_GRID_COLS) * *ch;
}

static inline void tb_colorgrid_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_colorgrid *g = (tb_colorgrid *)wb;
    for (int i = 0; i < 16; i++) {
        int cx, cy, cw, ch;
        tb_colorgrid_cell(wb, i, &cx, &cy, &cw, &ch);
        tb_fill(t, cx, cy, cw, ch, i);
        tb_frame(t, cx, cy, cw, ch, TB_C_FG);
        if (i == g->sel) {
            tb_frame(t, cx + 2, cy + 2, cw - 4, ch - 4, TB_C_FG);
            tb_frame(t, cx + 3, cy + 3, cw - 6, ch - 6, i == TB_C_FG ? TB_C_BG : TB_C_FG);
        }
    }
    if (focused) tb_frame(t, wb->x - 2, wb->y - 2, wb->w + 4, wb->h + 4, TB_C_FG);
}

static inline int tb_colorgrid_press(tb_app *t, tb_widget *wb, int x, int y) {
    tb_colorgrid *g = (tb_colorgrid *)wb;
    int cw = wb->w / TB_GRID_COLS, ch = wb->h / TB_GRID_ROWS;
    if (cw < 1 || ch < 1) return 0;
    int col = (x - wb->x) / cw, row = (y - wb->y) / ch;
    if (col < 0 || col >= TB_GRID_COLS || row < 0 || row >= TB_GRID_ROWS) return 0;
    int i = row * TB_GRID_COLS + col;
    if (i != g->sel) {
        g->sel = i;
        if (g->cb) g->cb(t, g->data);
    }
    return 0;
}

static inline void tb_colorgrid_destroy(tb_widget *wb) {
    free(wb);
}

static const tb_wops TB_COLORGRID_OPS = {
    tb_colorgrid_draw, tb_colorgrid_press, NULL, NULL, NULL, NULL,
    tb_colorgrid_destroy, NULL
};

static inline tb_colorgrid *tb_colorgrid_new(tb_app *t, int x, int y, int w, int h,
                                             tb_action cb, void *data) {
    tb_colorgrid *g = calloc(1, sizeof *g);
    if (!g) tb_oom();
    g->base.ops = &TB_COLORGRID_OPS;
    g->base.x = x;
    g->base.y = y;
    g->base.w = w;
    g->base.h = h;
    g->base.clickable = 1;
    g->base.focusable = 1;
    g->cb = cb;
    g->data = data;
    tb_add_widget(t, &g->base);
    return g;
}

/* Colour index 0..15 of the selected swatch. */
static inline int tb_colorgrid_get(const tb_colorgrid *g) {
    return g->sel;
}

static inline void tb_colorgrid_set(tb_colorgrid *g, int index) {
    if (index < 0 || index > 15) return;
    g->sel = index;
    if (g->base.app) tb_damage(g->base.app, &g->base);
}

#endif /* TB_COLORGRID_H */
