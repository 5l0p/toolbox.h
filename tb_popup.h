#ifndef TB_POPUP_H
#define TB_POPUP_H

/*
 * tb_popup.h - pop-up button (System 6 style). Press to open the list
 * below it, drag to the wanted item and release. Up/Down change the value
 * from the keyboard. The callback fires when the value changes.
 */

#include "tb_core.h"
#include "tb_scroll.h"

typedef struct tb_popup {
    tb_widget base;
    char **items;
    int nitems, citems;
    int value;
    int open, hover;
    tb_action cb;
    void *data;
} tb_popup;

static inline int tb_popup_list_h(const tb_popup *p) {
    return p->nitems * TB_MENU_ITEM_H + 4;
}

/* Index of the list item under (x, y), or -1. */
static inline int tb_popup_item_at(const tb_popup *p, int x, int y) {
    int ox = p->base.x, oy = p->base.y + p->base.h;
    if (x < ox || x >= ox + p->base.w) return -1;
    int rel = y - (oy + 2);
    if (rel < 0) return -1;
    int i = rel / TB_MENU_ITEM_H;
    return i < p->nitems ? i : -1;
}

static inline void tb_popup_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_popup *p = (tb_popup *)wb;
    tb_fill(t, wb->x, wb->y, wb->w, wb->h, TB_C_BG);
    tb_frame(t, wb->x, wb->y, wb->w, wb->h, TB_C_FG);
    if (focused)
        tb_frame(t, wb->x + 3, wb->y + 3, wb->w - 6, wb->h - 6, TB_C_FG);

    const char *cur = p->nitems ? p->items[p->value] : "";
    tb_str(t, wb->x + TB_MENU_PAD_X, tb_text_base(t, wb->y, wb->h), TB_C_FG, cur);

    /* arrow box on the right, separated by a vertical rule */
    int size = wb->h - 2;
    int ax = wb->x + wb->w - size - 1;
    tb_vline(t, ax - 2, wb->y + 1, wb->y + wb->h - 2, TB_C_FG);
    tb_sb_arrow(t, ax, wb->y + 1, size, 1, TB_C_FG);
}

static inline void tb_popup_overlay(tb_app *t, tb_widget *wb) {
    tb_popup *p = (tb_popup *)wb;
    int ox = wb->x, oy = wb->y + wb->h, ow = wb->w, oh = tb_popup_list_h(p);
    tb_fill(t, ox, oy, ow, oh, TB_C_BG);
    tb_frame(t, ox, oy, ow, oh, TB_C_FG);
    for (int i = 0; i < p->nitems; i++) {
        int iy = oy + 2 + i * TB_MENU_ITEM_H;
        int sel = (i == p->hover);
        if (sel) tb_fill(t, ox + 2, iy, ow - 4, TB_MENU_ITEM_H, TB_C_SEL_BG);
        tb_str(t, ox + TB_MENU_PAD_X, tb_text_base(t, iy, TB_MENU_ITEM_H),
               sel ? TB_C_SEL_FG : TB_C_FG, p->items[i]);
    }
}

static inline int tb_popup_press(tb_app *t, tb_widget *wb, int x, int y) {
    (void)x; (void)y;
    tb_popup *p = (tb_popup *)wb;
    if (p->nitems == 0) return 0;
    p->open = 1;
    p->hover = p->value;
    t->overlay = wb;
    return 1;   /* grab: motion and release come to us */
}

static inline void tb_popup_motion(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t;
    tb_popup *p = (tb_popup *)wb;
    if (!p->open) return;
    int i = tb_popup_item_at(p, x, y);
    if (i >= 0) p->hover = i;
}

static inline void tb_popup_set_value(tb_app *t, tb_popup *p, int v) {
    if (v < 0 || v >= p->nitems || v == p->value) return;
    p->value = v;
    if (p->cb) p->cb(t, p->data);
}

static inline void tb_popup_release(tb_app *t, tb_widget *wb, int x, int y) {
    tb_popup *p = (tb_popup *)wb;
    int i = tb_popup_item_at(p, x, y);
    p->open = 0;
    if (t->overlay == wb) t->overlay = NULL;
    if (i >= 0) tb_popup_set_value(t, p, i);
}

static inline void tb_popup_key(tb_app *t, tb_widget *wb, XKeyEvent *e) {
    tb_popup *p = (tb_popup *)wb;
    KeySym ks = XLookupKeysym(e, 0);
    if (ks == XK_Up)        tb_popup_set_value(t, p, p->value - 1);
    else if (ks == XK_Down) tb_popup_set_value(t, p, p->value + 1);
}

static inline void tb_popup_destroy(tb_widget *wb) {
    tb_popup *p = (tb_popup *)wb;
    for (int i = 0; i < p->nitems; i++) free(p->items[i]);
    free(p->items);
    free(p);
}

static const tb_wops TB_POPUP_OPS = {
    tb_popup_draw, tb_popup_press, tb_popup_motion, tb_popup_release,
    NULL, tb_popup_key, tb_popup_destroy, tb_popup_overlay
};

static inline tb_popup *tb_popup_new(tb_app *t, int x, int y, int w, int h,
                                     tb_action cb, void *data) {
    tb_popup *p = calloc(1, sizeof *p);
    if (!p) tb_oom();
    p->base.ops = &TB_POPUP_OPS;
    p->base.x = x;
    p->base.y = y;
    p->base.w = w;
    p->base.h = h;
    p->base.clickable = 1;
    p->base.focusable = 1;
    p->cb = cb;
    p->data = data;
    tb_add_widget(t, &p->base);
    return p;
}

static inline void tb_popup_add(tb_popup *p, const char *item) {
    char **slot = tb_push((void **)&p->items, &p->nitems, &p->citems, sizeof *slot);
    *slot = tb_strdup(item);
}

/* Select an item without firing the callback. */
static inline void tb_popup_set(tb_popup *p, int index) {
    if (index < 0) index = 0;
    if (index >= p->nitems) index = p->nitems - 1;
    p->value = index < 0 ? 0 : index;
    if (p->base.app) tb_damage(p->base.app, &p->base);
}

static inline int tb_popup_get(const tb_popup *p) {
    return p->value;
}

#endif /* TB_POPUP_H */
