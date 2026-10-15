#ifndef TB_SCROLLBAR_H
#define TB_SCROLLBAR_H

/*
 * tb_scrollbar.h - standalone scrollbar widget. The callback runs every
 * time the user moves it (arrow, track, thumb drag or mouse wheel).
 */

#include "tb_core.h"
#include "tb_scroll.h"

typedef struct tb_scrollbar {
    tb_widget base;
    tb_scroll sb;
    tb_action cb;
    void *data;
} tb_scrollbar;

static inline void tb_scrollbar_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_scrollbar *s = (tb_scrollbar *)wb;
    (void)focused;
    tb_sb_draw(t, &s->sb);
}

static inline int tb_scrollbar_press(tb_app *t, tb_widget *wb, int x, int y) {
    tb_scrollbar *s = (tb_scrollbar *)wb;
    if (tb_sb_press(&s->sb, x, y) && s->cb) s->cb(t, s->data);
    return 1;   /* grab so that thumb drags reach us */
}

static inline void tb_scrollbar_motion(tb_app *t, tb_widget *wb, int x, int y) {
    tb_scrollbar *s = (tb_scrollbar *)wb;
    if (s->sb.dragging && tb_sb_drag(&s->sb, x, y) && s->cb) s->cb(t, s->data);
}

static inline void tb_scrollbar_release(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t; (void)x; (void)y;
    ((tb_scrollbar *)wb)->sb.dragging = 0;
}

static inline void tb_scrollbar_wheel(tb_app *t, tb_widget *wb, int dir) {
    tb_scrollbar *s = (tb_scrollbar *)wb;
    if (tb_sb_adjust(&s->sb, dir * 3 * s->sb.step) && s->cb) s->cb(t, s->data);
}

static inline void tb_scrollbar_destroy(tb_widget *wb) {
    free(wb);
}

static const tb_wops TB_SCROLLBAR_OPS = {
    tb_scrollbar_draw, tb_scrollbar_press, tb_scrollbar_motion,
    tb_scrollbar_release, tb_scrollbar_wheel, NULL, tb_scrollbar_destroy, NULL
};

static inline tb_scrollbar *tb_scrollbar_new(tb_app *t, int x, int y, int w, int h,
                                             int vertical, tb_action cb, void *data) {
    tb_scrollbar *s = calloc(1, sizeof *s);
    if (!s) tb_oom();
    s->base.ops = &TB_SCROLLBAR_OPS;
    s->base.x = x;
    s->base.y = y;
    s->base.w = w;
    s->base.h = h;
    s->base.clickable = 1;
    s->sb.x = x;
    s->sb.y = y;
    s->sb.w = w;
    s->sb.h = h;
    s->sb.vertical = vertical ? 1 : 0;
    s->sb.total = 1;
    s->sb.page = 1;
    s->sb.step = 1;
    s->cb = cb;
    s->data = data;
    tb_add_widget(t, &s->base);
    return s;
}

/* total = size of content, page = visible amount, value = position. */
static inline void tb_scrollbar_set(tb_scrollbar *s, int total, int page, int value) {
    tb_sb_set(&s->sb, total, page);
    s->sb.value = value;
    tb_sb_clamp(&s->sb);
    if (s->base.app) tb_damage(s->base.app, &s->base);
}

static inline int tb_scrollbar_get(const tb_scrollbar *s) {
    return s->sb.value;
}

#endif /* TB_SCROLLBAR_H */
