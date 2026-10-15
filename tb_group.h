#ifndef TB_GROUP_H
#define TB_GROUP_H

/*
 * tb_group.h - decoration: a titled group box (frame with the title cut
 * into the top edge) and a rule (horizontal or vertical divider line).
 * Neither is clickable.
 */

#include "tb_core.h"

typedef struct tb_groupbox {
    tb_widget base;
    char *title;
} tb_groupbox;

static inline void tb_groupbox_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_groupbox *g = (tb_groupbox *)wb;
    (void)focused;
    int lh = t->fascent + t->fdescent;
    int top = wb->y + lh / 2;
    tb_frame(t, wb->x, top, wb->w, wb->h - lh / 2, TB_C_FG);
    if (g->title[0]) {
        int tw = tb_text_w(t, g->title);
        tb_fill(t, wb->x + 8, wb->y, tw + 6, lh, TB_C_BG);
        tb_str(t, wb->x + 11, tb_text_base(t, wb->y, lh), TB_C_FG, g->title);
    }
}

static inline void tb_groupbox_destroy(tb_widget *wb) {
    tb_groupbox *g = (tb_groupbox *)wb;
    free(g->title);
    free(g);
}

static const tb_wops TB_GROUPBOX_OPS = {
    tb_groupbox_draw, NULL, NULL, NULL, NULL, NULL, tb_groupbox_destroy, NULL
};

static inline tb_groupbox *tb_groupbox_new(tb_app *t, int x, int y, int w, int h,
                                           const char *title) {
    tb_groupbox *g = calloc(1, sizeof *g);
    if (!g) tb_oom();
    g->base.ops = &TB_GROUPBOX_OPS;
    g->base.x = x;
    g->base.y = y;
    g->base.w = w;
    g->base.h = h;
    g->title = tb_strdup(title ? title : "");
    tb_add_widget(t, &g->base);
    (void)t;
    return g;
}

/* A rule is a divider: horizontal if w >= h, vertical otherwise. */
static inline void tb_rule_draw(tb_app *t, tb_widget *wb, int focused) {
    (void)focused;
    if (wb->w >= wb->h) {
        int y = wb->y + wb->h / 2;
        tb_hline(t, wb->x, wb->x + wb->w - 1, y, TB_C_FG);
    } else {
        int x = wb->x + wb->w / 2;
        tb_vline(t, x, wb->y, wb->y + wb->h - 1, TB_C_FG);
    }
}

static inline void tb_rule_destroy(tb_widget *wb) {
    free(wb);
}

static const tb_wops TB_RULE_OPS = {
    tb_rule_draw, NULL, NULL, NULL, NULL, NULL, tb_rule_destroy, NULL
};

static inline void tb_rule_new(tb_app *t, int x, int y, int w, int h) {
    tb_widget *r = calloc(1, sizeof *r);
    if (!r) tb_oom();
    r->ops = &TB_RULE_OPS;
    r->x = x;
    r->y = y;
    r->w = w;
    r->h = h;
    tb_add_widget(t, r);
}

#endif /* TB_GROUP_H */
