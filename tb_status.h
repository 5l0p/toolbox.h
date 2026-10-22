#ifndef TB_STATUS_H
#define TB_STATUS_H

/* tb_status.h - status bar: a sunken strip showing one line of text. */

#include "tb_core.h"

typedef struct tb_status {
    tb_widget base;
    char *text;
} tb_status;

static inline void tb_status_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_status *s = (tb_status *)wb;
    (void)focused;
    tb_fill(t, wb->x, wb->y, wb->w, wb->h, TB_C_STATUS_BG);
    tb_frame(t, wb->x, wb->y, wb->w, wb->h, TB_C_FG);
    tb_str(t, wb->x + 6, tb_text_base(t, wb->y, wb->h), TB_C_STATUS_FG, s->text);
}

static inline void tb_status_destroy(tb_widget *wb) {
    tb_status *s = (tb_status *)wb;
    free(s->text);
    free(s);
}

static const tb_wops TB_STATUS_OPS = {
    tb_status_draw, NULL, NULL, NULL, NULL, NULL, tb_status_destroy, NULL
};

static inline tb_status *tb_status_new(tb_app *t, int x, int y, int w, int h,
                                       const char *text) {
    tb_status *s = calloc(1, sizeof *s);
    if (!s) tb_oom();
    s->base.ops = &TB_STATUS_OPS;
    s->base.x = x;
    s->base.y = y;
    s->base.w = w;
    s->base.h = h;
    s->text = tb_strdup(text ? text : "");
    tb_add_widget(t, &s->base);
    return s;
}

static inline void tb_status_set(tb_status *s, const char *text) {
    free(s->text);
    s->text = tb_strdup(text ? text : "");
    if (s->base.app) tb_damage(s->base.app, &s->base);
}

#endif /* TB_STATUS_H */
