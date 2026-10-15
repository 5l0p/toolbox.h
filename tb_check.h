#ifndef TB_CHECK_H
#define TB_CHECK_H

/*
 * tb_check.h - checkbox. Click or Space toggles; the callback fires when
 * the state changes. The box is drawn with an X when checked.
 */

#include "tb_core.h"

#define TB_CHECK_BOX 13

typedef struct tb_checkbox {
    tb_widget base;
    char *text;
    int checked;
    int pressed, inside;
    tb_action cb;
    void *data;
} tb_checkbox;

static inline void tb_checkbox_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_checkbox *c = (tb_checkbox *)wb;
    int bx = wb->x;
    int by = wb->y + (wb->h - TB_CHECK_BOX) / 2;
    int down = c->pressed && c->inside;
    tb_fill(t, bx, by, TB_CHECK_BOX, TB_CHECK_BOX, down ? TB_C_FG : TB_C_BG);
    tb_frame(t, bx, by, TB_CHECK_BOX, TB_CHECK_BOX, TB_C_FG);
    if (c->checked) {
        tb_draw_line(t, bx + 3, by + 3, bx + TB_CHECK_BOX - 4, by + TB_CHECK_BOX - 4,
                down ? TB_C_BG : TB_C_FG);
        tb_draw_line(t, bx + TB_CHECK_BOX - 4, by + 3, bx + 3, by + TB_CHECK_BOX - 4,
                down ? TB_C_BG : TB_C_FG);
    }
    tb_str(t, bx + TB_CHECK_BOX + 6, tb_text_base(t, wb->y, wb->h), TB_C_FG, c->text);
    if (focused)
        tb_frame(t, wb->x - 2, wb->y, wb->w + 4, wb->h, TB_C_FG);
}

static inline void tb_checkbox_toggle(tb_app *t, tb_checkbox *c) {
    c->checked = !c->checked;
    if (c->cb) c->cb(t, c->data);
}

static inline int tb_checkbox_press(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t; (void)x; (void)y;
    tb_checkbox *c = (tb_checkbox *)wb;
    c->pressed = 1;
    c->inside = 1;
    return 1;
}

static inline void tb_checkbox_motion(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t;
    tb_checkbox *c = (tb_checkbox *)wb;
    c->inside = tb_inside(wb, x, y);
}

static inline void tb_checkbox_release(tb_app *t, tb_widget *wb, int x, int y) {
    (void)x; (void)y;
    tb_checkbox *c = (tb_checkbox *)wb;
    int fire = c->inside;
    c->pressed = 0;
    c->inside = 0;
    if (fire) tb_checkbox_toggle(t, c);
}

static inline void tb_checkbox_key(tb_app *t, tb_widget *wb, XKeyEvent *e) {
    KeySym ks = XLookupKeysym(e, 0);
    if (ks == XK_space || ks == XK_Return || ks == XK_KP_Enter)
        tb_checkbox_toggle(t, (tb_checkbox *)wb);
}

static inline void tb_checkbox_destroy(tb_widget *wb) {
    tb_checkbox *c = (tb_checkbox *)wb;
    free(c->text);
    free(c);
}

static const tb_wops TB_CHECKBOX_OPS = {
    tb_checkbox_draw, tb_checkbox_press, tb_checkbox_motion, tb_checkbox_release,
    NULL, tb_checkbox_key, tb_checkbox_destroy, NULL
};

static inline tb_checkbox *tb_checkbox_new(tb_app *t, int x, int y, const char *label,
                                           tb_action cb, void *data) {
    tb_checkbox *c = calloc(1, sizeof *c);
    if (!c) tb_oom();
    c->base.ops = &TB_CHECKBOX_OPS;
    c->base.x = x;
    c->base.y = y;
    c->base.w = TB_CHECK_BOX + 6 + tb_text_w(t, label);
    c->base.h = t->fascent + t->fdescent + 6;
    c->base.clickable = 1;
    c->base.focusable = 1;
    c->text = tb_strdup(label);
    c->cb = cb;
    c->data = data;
    tb_add_widget(t, &c->base);
    return c;
}

static inline void tb_checkbox_set(tb_checkbox *c, int checked) {
    c->checked = checked ? 1 : 0;
    if (c->base.app) tb_damage(c->base.app, &c->base);
}

static inline int tb_checkbox_get(const tb_checkbox *c) {
    return c->checked;
}

#endif /* TB_CHECK_H */
