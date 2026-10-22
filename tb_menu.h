#ifndef TB_MENU_H
#define TB_MENU_H

/*
 * tb_menu.h - System 6 style menubar, drawn inside the window.
 * Menus are created with tb_menu_new() / tb_menu_item() in toolbox.h.
 */

#include "tb_core.h"

static inline void tb_menu_layout(tb_app *t) {
    int x = TB_MENU_PAD_X;
    for (int i = 0; i < t->nmenus; i++) {
        tb_menu *m = &t->menus[i];
        m->x = x;
        m->tw = tb_text_w(t, m->title) + 2 * TB_MENU_PAD_X;
        x += m->tw;

        int maxw = 0;
        for (int j = 0; j < m->nitems; j++) {
            if (!m->items[j].title) continue;
            int w = tb_text_w(t, m->items[j].title);
            if (w > maxw) maxw = w;
        }
        m->dw = maxw + 4 * TB_MENU_PAD_X;
        if (m->dw < TB_MENU_MIN_W) m->dw = TB_MENU_MIN_W;
    }
}

static inline int tb_menu_at(tb_app *t, int x, int y) {
    if (y < 0 || y >= TB_MENUBAR_H) return -1;
    for (int i = 0; i < t->nmenus; i++)
        if (x >= t->menus[i].x && x < t->menus[i].x + t->menus[i].tw)
            return i;
    return -1;
}

static inline int tb_menu_in_dropdown(tb_app *t, int x, int y) {
    if (t->open_menu < 0) return 0;
    tb_menu *m = &t->menus[t->open_menu];
    int h = m->nitems * TB_MENU_ITEM_H + 4;
    return x >= m->x && x < m->x + m->dw &&
           y >= TB_MENUBAR_H && y < TB_MENUBAR_H + h;
}

static inline int tb_menu_item_at(tb_app *t, int x, int y) {
    if (!tb_menu_in_dropdown(t, x, y)) return -1;
    int rel = y - (TB_MENUBAR_H + 2);
    if (rel < 0) return -1;
    int i = rel / TB_MENU_ITEM_H;
    return i < t->menus[t->open_menu].nitems ? i : -1;
}

static inline void tb_menu_close(tb_app *t) {
    t->open_menu = -1;
    t->open_item = -1;
}

/* Returns the menu's index. */
static inline int tb_menu_new(tb_app *t, const char *title) {
    tb_menu *m = tb_push((void **)&t->menus, &t->nmenus, &t->cmenus, sizeof *m);
    m->title = tb_strdup(title);
    tb_menu_layout(t);
    return t->nmenus - 1;
}

/* Title "-" makes a separator. Returns the item's index in its menu, or -1. */
static inline int tb_menu_item(tb_app *t, const char *title, tb_action cb, void *data) {
    if (t->nmenus == 0) return -1;
    tb_menu *m = &t->menus[t->nmenus - 1];
    tb_item *it = tb_push((void **)&m->items, &m->nitems, &m->citems, sizeof *it);
    it->title = strcmp(title, "-") == 0 ? NULL : tb_strdup(title);
    it->cb = cb;
    it->data = data;
    tb_menu_layout(t);
    return m->nitems - 1;
}

/* Change the text of an existing item ("-" makes it a separator). Use this
 * for toggles, e.g. "Show Hidden Files" <-> "Hide Hidden Files". */
static inline void tb_menu_set_item(tb_app *t, int menu, int item, const char *title) {
    tb_item *it = &t->menus[menu].items[item];
    free(it->title);
    it->title = strcmp(title, "-") == 0 ? NULL : tb_strdup(title);
    tb_menu_layout(t);
}

static inline void tb_menu_draw_bar(tb_app *t) {
    tb_fill(t, 0, 0, t->width, TB_MENUBAR_H, TB_C_MENUBAR);
    tb_hline(t, 0, t->width - 1, TB_MENUBAR_H - 1, TB_C_FG);
    for (int i = 0; i < t->nmenus; i++) {
        tb_menu *m = &t->menus[i];
        int sel = (i == t->open_menu);
        if (sel) tb_fill(t, m->x, 0, m->tw, TB_MENUBAR_H - 1, TB_C_SEL_BG);
        tb_str(t, m->x + TB_MENU_PAD_X, tb_text_base(t, 0, TB_MENUBAR_H),
               sel ? TB_C_SEL_FG : TB_C_FG, m->title);
    }
    if (t->bar_status) {
        int w = tb_text_w(t, t->bar_status);
        int x = t->width - TB_MENU_PAD_X - w;
        if (x > 0)
            tb_str(t, x, tb_text_base(t, 0, TB_MENUBAR_H), TB_C_FG, t->bar_status);
    }
}

/* Text shown at the right of the menubar (NULL or "" clears it). Not a
 * menu: it is display-only and takes no clicks. */
static inline void tb_menu_status(tb_app *t, const char *text) {
    free(t->bar_status);
    t->bar_status = (text && *text) ? tb_strdup(text) : NULL;
    tb_damage_rect(t, 0, 0, t->width, TB_MENUBAR_H);
}

/* Drawn after all widgets so it sits on top. */
static inline void tb_menu_draw_dropdown(tb_app *t) {
    if (t->open_menu < 0) return;
    tb_menu *m = &t->menus[t->open_menu];
    int dh = m->nitems * TB_MENU_ITEM_H + 4;
    int dy = TB_MENUBAR_H;
    tb_fill(t, m->x, dy, m->dw, dh, TB_C_BG);
    tb_frame(t, m->x, dy, m->dw, dh, TB_C_FG);
    for (int j = 0; j < m->nitems; j++) {
        int iy = dy + 2 + j * TB_MENU_ITEM_H;
        tb_item *it = &m->items[j];
        if (!it->title) {
            tb_hline(t, m->x + 1, m->x + m->dw - 2,
                     iy + TB_MENU_ITEM_H / 2, TB_C_FG);
            continue;
        }
        int sel = (j == t->open_item);
        if (sel) tb_fill(t, m->x + 2, iy, m->dw - 4, TB_MENU_ITEM_H, TB_C_SEL_BG);
        tb_str(t, m->x + 2 * TB_MENU_PAD_X, tb_text_base(t, iy, TB_MENU_ITEM_H),
               sel ? TB_C_SEL_FG : TB_C_FG, it->title);
    }
}

/* Pointer handling. Each returns 1 if the event was consumed by the menus. */

static inline int tb_menu_press(tb_app *t, int x, int y) {
    if (y < TB_MENUBAR_H) {
        int m = tb_menu_at(t, x, y);
        t->open_item = -1;
        t->open_menu = (m == t->open_menu) ? -1 : m;
        return 1;
    }
    if (t->open_menu < 0) return 0;
    if (tb_menu_in_dropdown(t, x, y)) t->open_item = tb_menu_item_at(t, x, y);
    else tb_menu_close(t);
    return 1;
}

static inline int tb_menu_motion(tb_app *t, int x, int y) {
    if (t->open_menu < 0) return 0;
    /* Like System 6: sliding across the bar switches menus */
    int m = tb_menu_at(t, x, y);
    if (m >= 0 && m != t->open_menu) {
        t->open_menu = m;
        t->open_item = -1;
        return 1;
    }
    t->open_item = tb_menu_item_at(t, x, y);
    return 1;
}

static inline int tb_menu_release(tb_app *t, int x, int y) {
    if (t->open_menu < 0) return 0;
    int i = tb_menu_item_at(t, x, y);
    tb_menu *m = &t->menus[t->open_menu];
    if (i >= 0 && m->items[i].title) {
        tb_item it = m->items[i];
        tb_menu_close(t);
        if (it.cb) it.cb(t, it.data);
    }
    /* otherwise the menu stays open for click-click use */
    return 1;
}

static inline void tb_menu_free(tb_menu *menus, int n) {
    for (int i = 0; i < n; i++) {
        free(menus[i].title);
        for (int j = 0; j < menus[i].nitems; j++)
            free(menus[i].items[j].title);
        free(menus[i].items);
    }
    free(menus);
}

#endif /* TB_MENU_H */
