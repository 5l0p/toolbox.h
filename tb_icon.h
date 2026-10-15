#ifndef TB_ICON_H
#define TB_ICON_H

/*
 * tb_icon.h - static 1-bit bitmap. Rows are (w+7)/8 bytes, most
 * significant bit first; a set bit is drawn in the chosen palette colour.
 * The bitmap is not copied, so it must outlive the icon.
 */

#include "tb_core.h"

typedef struct tb_icon {
    tb_widget base;
    const unsigned char *bits;
    int color;
} tb_icon;

static inline void tb_icon_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_icon *ic = (tb_icon *)wb;
    (void)focused;
    int rowbytes = (wb->w + 7) / 8;
    for (int y = 0; y < wb->h; y++) {
        const unsigned char *row = ic->bits + y * rowbytes;
        int x = 0;
        while (x < wb->w) {       /* draw runs of set bits as rectangles */
            if (!((row[x >> 3] >> (7 - (x & 7))) & 1)) { x++; continue; }
            int start = x;
            while (x < wb->w && ((row[x >> 3] >> (7 - (x & 7))) & 1)) x++;
            tb_fill(t, wb->x + start, wb->y + y, x - start, 1, ic->color);
        }
    }
}

static inline void tb_icon_destroy(tb_widget *wb) {
    free(wb);
}

static const tb_wops TB_ICON_OPS = {
    tb_icon_draw, NULL, NULL, NULL, NULL, NULL, tb_icon_destroy, NULL
};

static inline tb_icon *tb_icon_new(tb_app *t, int x, int y, int w, int h,
                                   const unsigned char *bits, int color) {
    tb_icon *ic = calloc(1, sizeof *ic);
    if (!ic) tb_oom();
    ic->base.ops = &TB_ICON_OPS;
    ic->base.x = x;
    ic->base.y = y;
    ic->base.w = w;
    ic->base.h = h;
    ic->bits = bits;
    ic->color = color & 15;
    tb_add_widget(t, &ic->base);
    return ic;
}

#endif /* TB_ICON_H */
