#ifndef TB_TABS_H
#define TB_TABS_H

/*
 * tb_tabs.h - tab strip. The tabs only report the selection; the caller
 * shows and hides its own widgets (tb_widget_hide) in the callback, which
 * keeps layout under the caller's control. Left/Right keys change tabs.
 */

#include "tb_core.h"

#define TB_TAB_PAD 8

typedef struct tb_tabs {
    tb_widget base;
    char **titles;
    int n, cap;
    int cur;
    tb_action cb;
    void *data;
} tb_tabs;

static inline int tb_tabs_strip_h(const tb_app *t) {
    return t->fascent + t->fdescent + 8;
}

/* Width of tab i including its padding. */
static inline int tb_tabs_tab_w(tb_app *t, const tb_tabs *tb, int i) {
    return tb_text_w(t, tb->titles[i]) + 2 * TB_TAB_PAD;
}

static inline void tb_tabs_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_tabs *tb = (tb_tabs *)wb;
    (void)focused;
    int sh = tb_tabs_strip_h(t);
    tb_fill(t, wb->x, wb->y, wb->w, wb->h, TB_C_BG);
    /* body frame below the strip */
    tb_frame(t, wb->x, wb->y + sh - 1, wb->w, wb->h - sh + 1, TB_C_FG);
    int x = wb->x;
    for (int i = 0; i < tb->n; i++) {
        int tw = tb_tabs_tab_w(t, tb, i);
        int sel = (i == tb->cur);
        if (sel) tb_fill(t, x, wb->y, tw, sh, TB_C_FG);
        else     tb_fill(t, x, wb->y, tw, sh, TB_C_BG);
        tb_frame(t, x, wb->y, tw, sh, TB_C_FG);
        tb_str(t, x + TB_TAB_PAD, tb_text_base(t, wb->y, sh),
               sel ? TB_C_BG : TB_C_FG, tb->titles[i]);
        if (sel) tb_hline(t, x + 1, x + tw - 2, wb->y + sh - 1, TB_C_FG);
        x += tw;
    }
}

static inline void tb_tabs_select(tb_app *t, tb_tabs *tb, int i) {
    if (i < 0 || i >= tb->n || i == tb->cur) return;
    tb->cur = i;
    if (tb->cb) tb->cb(t, tb->data);
}

static inline int tb_tabs_press(tb_app *t, tb_widget *wb, int x, int y) {
    tb_tabs *tb = (tb_tabs *)wb;
    if (y >= wb->y + tb_tabs_strip_h(t)) return 0;
    int acc = wb->x;
    for (int i = 0; i < tb->n; i++) {
        int tw = tb_tabs_tab_w(t, tb, i);
        if (x >= acc && x < acc + tw) {
            tb_tabs_select(t, tb, i);
            break;
        }
        acc += tw;
    }
    return 0;
}

static inline void tb_tabs_key(tb_app *t, tb_widget *wb, XKeyEvent *e) {
    tb_tabs *tb = (tb_tabs *)wb;
    KeySym ks = XLookupKeysym(e, 0);
    if (ks == XK_Left)       tb_tabs_select(t, tb, tb->cur - 1);
    else if (ks == XK_Right) tb_tabs_select(t, tb, tb->cur + 1);
}

static inline void tb_tabs_destroy(tb_widget *wb) {
    tb_tabs *tb = (tb_tabs *)wb;
    for (int i = 0; i < tb->n; i++) free(tb->titles[i]);
    free(tb->titles);
    free(tb);
}

static const tb_wops TB_TABS_OPS = {
    tb_tabs_draw, tb_tabs_press, NULL, NULL,
    NULL, tb_tabs_key, tb_tabs_destroy, NULL
};

static inline tb_tabs *tb_tabs_new(tb_app *t, int x, int y, int w, int h,
                                   tb_action cb, void *data) {
    tb_tabs *tb = calloc(1, sizeof *tb);
    if (!tb) tb_oom();
    tb->base.ops = &TB_TABS_OPS;
    tb->base.x = x;
    tb->base.y = y;
    tb->base.w = w;
    tb->base.h = h;
    tb->base.clickable = 1;
    tb->base.focusable = 1;
    tb->cb = cb;
    tb->data = data;
    tb_add_widget(t, &tb->base);
    return tb;
}

static inline void tb_tabs_add(tb_tabs *tb, const char *title) {
    char **slot = tb_push((void **)&tb->titles, &tb->n, &tb->cap, sizeof *slot);
    *slot = tb_strdup(title);
}

/* Select a tab without firing the callback. */
static inline void tb_tabs_set(tb_tabs *tb, int i) {
    if (i < 0) i = 0;
    if (i >= tb->n) i = tb->n - 1;
    tb->cur = i < 0 ? 0 : i;
    if (tb->base.app) tb_damage(tb->base.app, &tb->base);
}

static inline int tb_tabs_get(const tb_tabs *tb) {
    return tb->cur;
}

/* Change the title of tab i. */
static inline void tb_tabs_rename(tb_tabs *tb, int i, const char *title) {
    if (i < 0 || i >= tb->n) return;
    free(tb->titles[i]);
    tb->titles[i] = tb_strdup(title);
    if (tb->base.app) tb_damage(tb->base.app, &tb->base);
}

/* Remove tab i. The selection stays on the same index, or the last tab. */
static inline void tb_tabs_remove(tb_tabs *tb, int i) {
    if (i < 0 || i >= tb->n) return;
    free(tb->titles[i]);
    memmove(&tb->titles[i], &tb->titles[i + 1],
            (size_t)(tb->n - i - 1) * sizeof *tb->titles);
    tb->n--;
    if (tb->cur >= tb->n) tb->cur = tb->n > 0 ? tb->n - 1 : 0;
    if (tb->base.app) tb_damage(tb->base.app, &tb->base);
}

#endif /* TB_TABS_H */
