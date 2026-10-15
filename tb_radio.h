#ifndef TB_RADIO_H
#define TB_RADIO_H

/*
 * tb_radio.h - radio buttons. Radios form a group: pass NULL to start a
 * new group, or any existing radio to join its group. Selecting one
 * deselects the others. The first radio of a new group starts selected.
 * The callback fires when the user picks a radio that was not selected.
 */

#include "tb_core.h"

#define TB_RADIO_D 13

typedef struct tb_radio {
    tb_widget base;
    char *text;
    int on;
    int pressed, inside;
    tb_action cb;
    void *data;
    struct tb_radio *next;   /* circular list of the group */
} tb_radio;

static inline void tb_radio_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_radio *r = (tb_radio *)wb;
    int dx = wb->x, dy = wb->y + (wb->h - TB_RADIO_D) / 2;
    int down = r->pressed && r->inside;
    tb_arc(t, dx, dy, TB_RADIO_D, TB_RADIO_D, down ? TB_C_FG : TB_C_BG, 1);
    tb_arc(t, dx, dy, TB_RADIO_D, TB_RADIO_D, TB_C_FG, 0);
    if (r->on)
        tb_arc(t, dx + 4, dy + 4, TB_RADIO_D - 8, TB_RADIO_D - 8,
               down ? TB_C_BG : TB_C_FG, 1);
    tb_str(t, dx + TB_RADIO_D + 6, tb_text_base(t, wb->y, wb->h), TB_C_FG, r->text);
    if (focused)
        tb_frame(t, wb->x - 2, wb->y, wb->w + 4, wb->h, TB_C_FG);
}

/* Make r the only selected radio in its group. */
static inline void tb_radio_select(tb_radio *r) {
    tb_radio *p = r;
    do {
        p->on = 0;
        p = p->next;
    } while (p != r);
    r->on = 1;
}

static inline void tb_radio_fire(tb_app *t, tb_radio *r) {
    if (r->on) return;
    tb_radio_select(r);
    if (r->cb) r->cb(t, r->data);
}

static inline int tb_radio_press(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t; (void)x; (void)y;
    tb_radio *r = (tb_radio *)wb;
    r->pressed = 1;
    r->inside = 1;
    return 1;
}

static inline void tb_radio_motion(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t;
    tb_radio *r = (tb_radio *)wb;
    r->inside = tb_inside(wb, x, y);
}

static inline void tb_radio_release(tb_app *t, tb_widget *wb, int x, int y) {
    (void)x; (void)y;
    tb_radio *r = (tb_radio *)wb;
    int fire = r->inside;
    r->pressed = 0;
    r->inside = 0;
    if (fire) tb_radio_fire(t, r);
}

static inline void tb_radio_key(tb_app *t, tb_widget *wb, XKeyEvent *e) {
    KeySym ks = XLookupKeysym(e, 0);
    if (ks == XK_space || ks == XK_Return || ks == XK_KP_Enter)
        tb_radio_fire(t, (tb_radio *)wb);
}

/* Unlink from the group ring before freeing. */
static inline void tb_radio_destroy(tb_widget *wb) {
    tb_radio *r = (tb_radio *)wb;
    if (r->next != r) {
        tb_radio *p = r->next;
        while (p->next != r) p = p->next;
        p->next = r->next;
    }
    free(r->text);
    free(r);
}

static const tb_wops TB_RADIO_OPS = {
    tb_radio_draw, tb_radio_press, tb_radio_motion, tb_radio_release,
    NULL, tb_radio_key, tb_radio_destroy, NULL
};

/* group = NULL starts a new group; any member joins that member's group. */
static inline tb_radio *tb_radio_new(tb_app *t, tb_radio *group, int x, int y,
                                     const char *label, tb_action cb, void *data) {
    tb_radio *r = calloc(1, sizeof *r);
    if (!r) tb_oom();
    r->base.ops = &TB_RADIO_OPS;
    r->base.x = x;
    r->base.y = y;
    r->base.w = TB_RADIO_D + 6 + tb_text_w(t, label);
    r->base.h = t->fascent + t->fdescent + 6;
    r->base.clickable = 1;
    r->base.focusable = 1;
    r->text = tb_strdup(label);
    r->cb = cb;
    r->data = data;
    if (group) {
        r->next = group->next;
        group->next = r;
    } else {
        r->next = r;
        r->on = 1;
    }
    tb_add_widget(t, &r->base);
    return r;
}

static inline int tb_radio_selected(const tb_radio *r) {
    return r->on;
}

/* Index (0-based, in group order) of the selected radio, starting from r. */
static inline int tb_radio_group_value(const tb_radio *r) {
    const tb_radio *p = r;
    int i = 0;
    do {
        if (p->on) return i;
        p = p->next;
        i++;
    } while (p != r);
    return -1;
}

#endif /* TB_RADIO_H */
