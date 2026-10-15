#ifndef TB_COMBO_H
#define TB_COMBO_H

/*
 * tb_combo.h - combo box: an editable single-line field with a drop-down
 * arrow. Clicking the arrow opens the list (like a pop-up button); choosing
 * an item copies it into the field and fires the callback. Typing and the
 * field's clipboard shortcuts work as in tb_field.
 */

#include "tb_field.h"

#define TB_COMBO_AW 16

typedef struct tb_combo {
    tb_field f;               /* first member: its base is the combo's base */
    char **items;
    int nitems, citems;
    int open, hover;
    int mode;                 /* 0 = editing the field, 1 = choosing from list */
    tb_action cb;
    void *data;
} tb_combo;

static inline int tb_combo_list_h(const tb_combo *c) {
    return c->nitems * TB_MENU_ITEM_H + 4;
}

static inline int tb_combo_item_at(const tb_combo *c, int x, int y) {
    int ox = c->f.base.x, oy = c->f.base.y + c->f.base.h;
    if (x < ox || x >= ox + c->f.base.w) return -1;
    int rel = y - (oy + 2);
    if (rel < 0) return -1;
    int i = rel / TB_MENU_ITEM_H;
    return i < c->nitems ? i : -1;
}

static inline void tb_combo_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_combo *c = (tb_combo *)wb;
    tb_field_draw(t, wb, focused);
    int ax = wb->x + wb->w - TB_COMBO_AW;
    tb_fill(t, ax, wb->y, TB_COMBO_AW, wb->h, TB_C_BG);
    tb_frame(t, ax, wb->y, TB_COMBO_AW, wb->h, TB_C_FG);
    int cx = ax + TB_COMBO_AW / 2, cy = wb->y + wb->h / 2;
    XPoint p[3];
    p[0].x = (short)cx;       p[0].y = (short)(cy + 3);
    p[1].x = (short)(cx - 4); p[1].y = (short)(cy - 2);
    p[2].x = (short)(cx + 4); p[2].y = (short)(cy - 2);
    tb_poly(t, p, 3, TB_C_FG);
    (void)c;
}

static inline void tb_combo_overlay(tb_app *t, tb_widget *wb) {
    tb_combo *c = (tb_combo *)wb;
    int ox = wb->x, oy = wb->y + wb->h, ow = wb->w, oh = tb_combo_list_h(c);
    tb_fill(t, ox, oy, ow, oh, TB_C_BG);
    tb_frame(t, ox, oy, ow, oh, TB_C_FG);
    for (int i = 0; i < c->nitems; i++) {
        int iy = oy + 2 + i * TB_MENU_ITEM_H;
        int sel = (i == c->hover);
        if (sel) tb_fill(t, ox + 2, iy, ow - 4, TB_MENU_ITEM_H, TB_C_SEL_BG);
        tb_str(t, ox + TB_MENU_PAD_X, tb_text_base(t, iy, TB_MENU_ITEM_H),
               sel ? TB_C_SEL_FG : TB_C_FG, c->items[i]);
    }
}

static inline int tb_combo_press(tb_app *t, tb_widget *wb, int x, int y) {
    tb_combo *c = (tb_combo *)wb;
    if (x >= wb->x + wb->w - TB_COMBO_AW) {
        if (c->nitems == 0) return 0;
        c->mode = 1;
        c->open = 1;
        c->hover = 0;
        t->overlay = wb;
        return 1;
    }
    c->mode = 0;
    return tb_field_press(t, wb, x, y);
}

static inline void tb_combo_motion(tb_app *t, tb_widget *wb, int x, int y) {
    tb_combo *c = (tb_combo *)wb;
    if (c->mode == 1) {
        int i = tb_combo_item_at(c, x, y);
        if (i >= 0) c->hover = i;
    } else {
        tb_field_motion(t, wb, x, y);
    }
}

static inline void tb_combo_release(tb_app *t, tb_widget *wb, int x, int y) {
    tb_combo *c = (tb_combo *)wb;
    if (c->mode == 1) {
        int i = tb_combo_item_at(c, x, y);
        c->open = 0;
        c->mode = 0;
        if (t->overlay == wb) t->overlay = NULL;
        if (i >= 0) {
            tb_field_set(&c->f, c->items[i]);
            if (c->cb) c->cb(t, c->data);
        }
    } else {
        tb_field_release(t, wb, x, y);
    }
}

static inline void tb_combo_key(tb_app *t, tb_widget *wb, XKeyEvent *e) {
    tb_field_key(t, wb, e);
}

static inline void tb_combo_destroy(tb_widget *wb) {
    tb_combo *c = (tb_combo *)wb;
    for (int i = 0; i < c->nitems; i++) free(c->items[i]);
    free(c->items);
    free(c->f.s);
    free(c);
}

static const tb_wops TB_COMBO_OPS = {
    tb_combo_draw, tb_combo_press, tb_combo_motion, tb_combo_release,
    NULL, tb_combo_key, tb_combo_destroy, tb_combo_overlay
};

static inline tb_combo *tb_combo_new(tb_app *t, int x, int y, int w, int h,
                                     tb_action cb, void *data) {
    tb_combo *c = calloc(1, sizeof *c);
    if (!c) tb_oom();
    c->f.base.ops = &TB_COMBO_OPS;
    c->f.base.x = x;
    c->f.base.y = y;
    c->f.base.w = w;
    c->f.base.h = h;
    c->f.base.clickable = 1;
    c->f.base.focusable = 1;
    c->f.base.text_in = tb_field_text_in;
    c->f.rightpad = TB_COMBO_AW;
    c->f.cap = 32;
    c->f.s = malloc((size_t)c->f.cap);
    if (!c->f.s) tb_oom();
    c->f.s[0] = 0;
    c->cb = cb;
    c->data = data;
    tb_add_widget(t, &c->f.base);
    return c;
}

static inline void tb_combo_add(tb_combo *c, const char *item) {
    char **slot = tb_push((void **)&c->items, &c->nitems, &c->citems, sizeof *slot);
    *slot = tb_strdup(item);
}

/* Set the text in the field without firing the callback. */
static inline void tb_combo_set(tb_combo *c, const char *text) {
    tb_field_set(&c->f, text);
}

static inline const char *tb_combo_get(const tb_combo *c) {
    return tb_field_get(&c->f);
}

#endif /* TB_COMBO_H */
