#ifndef TB_LABEL_H
#define TB_LABEL_H

/* tb_label.h - static text in a palette colour. Not clickable. */

#include "tb_core.h"

typedef struct tb_label {
    tb_widget base;
    char *text;
    int color;
} tb_label;

static inline void tb_label_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_label *l = (tb_label *)wb;
    (void)focused;
    tb_str(t, l->base.x, tb_text_base(t, l->base.y, l->base.h), l->color, l->text);
}

static inline void tb_label_destroy(tb_widget *wb) {
    tb_label *l = (tb_label *)wb;
    free(l->text);
    free(l);
}

static const tb_wops TB_LABEL_OPS = {
    tb_label_draw, NULL, NULL, NULL, NULL, NULL, tb_label_destroy
};

static inline tb_label *tb_label_new(tb_app *t, int x, int y, int color, const char *s) {
    tb_label *l = calloc(1, sizeof *l);
    if (!l) tb_oom();
    l->base.ops = &TB_LABEL_OPS;
    l->base.x = x;
    l->base.y = y;
    l->base.w = tb_text_w(t, s);
    l->base.h = t->fascent + t->fdescent;
    l->color = color & 15;
    l->text = tb_strdup(s);
    tb_add_widget(t, &l->base);
    return l;
}

static inline void tb_label_set(tb_app *t, tb_label *l, const char *s) {
    free(l->text);
    l->text = tb_strdup(s);
    l->base.w = tb_text_w(t, s);
}

#endif /* TB_LABEL_H */
