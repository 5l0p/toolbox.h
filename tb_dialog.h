#ifndef TB_DIALOG_H
#define TB_DIALOG_H

/*
 * tb_dialog.h - modal dialog. A dialog owns its own widgets and focus.
 * While it is open (tb_dialog_run) all input goes to it and a nested
 * event loop runs until one of its buttons, or Escape, sets the result.
 *
 * Typical use:
 *   tb_dialog *d = tb_dialog_new(t, x, y, w, h, "Title");
 *   tb_container *prev = tb_build_begin(t, &d->c);
 *   tb_dialog_button(t, x, y, w, h, "OK", 1);       // value 1
 *   tb_dialog_set_default(d, b);                    // Return activates it
 *   tb_build_end(t, prev);
 *   int r = tb_dialog_run(t, d);
 *   tb_dialog_free(d);
 *
 * Widgets built inside the dialog use screen coordinates.
 */

#include "tb_core.h"
#include "tb_button.h"

static inline tb_dialog *tb_dialog_new(tb_app *t, int x, int y, int w, int h,
                                       const char *title) {
    tb_dialog *d = calloc(1, sizeof *d);
    if (!d) tb_oom();
    (void)t;
    d->x = x;
    d->y = y;
    d->w = w;
    d->h = h;
    d->title = tb_strdup(title ? title : "");
    d->result = -1;
    d->cancel_value = -1;
    return d;
}

/* Route new widgets into c (usually &d->c). Returns the previous target;
 * pass it back to tb_build_end(). */
static inline tb_container *tb_build_begin(tb_app *t, tb_container *c) {
    tb_container *prev = t->build;
    t->build = c;
    return prev;
}

static inline void tb_build_end(tb_app *t, tb_container *prev) {
    t->build = prev;
}

static inline void tb_dialog_close(tb_app *t, tb_dialog *d, int result) {
    (void)t;
    d->result = result;
}

/* Button callback: closes whichever dialog is modal with this value. */
static inline void tb_dialog_btn_cb(tb_app *t, void *data) {
    if (t->modal) tb_dialog_close(t, t->modal, (int)(intptr_t)data);
}

/* A button that closes the dialog with `value` (must be >= 0). */
static inline tb_button *tb_dialog_button(tb_app *t, int x, int y, int w, int h,
                                          const char *label, int value) {
    return tb_button_new(t, x, y, w, h, label, tb_dialog_btn_cb,
                         (void *)(intptr_t)value);
}

/* Return activates this button. */
static inline void tb_dialog_set_default(tb_dialog *d, tb_button *b) {
    d->def = &b->base;
}

/* Escape closes the dialog with this value. */
static inline void tb_dialog_set_cancel(tb_dialog *d, int value) {
    d->cancel_value = value;
}

static inline void tb_dialog_paint(tb_app *t, tb_dialog *d) {
    t->ox = 0;
    t->oy = 0;
    tb_fill(t, d->x, d->y, d->w, d->h, TB_C_BG);
    tb_frame(t, d->x, d->y, d->w, d->h, TB_C_FG);
    tb_frame(t, d->x + 3, d->y + 3, d->w - 6, d->h - 6, TB_C_FG);
    if (d->title[0]) {
        int lh = t->fascent + t->fdescent;
        int tw = tb_text_w(t, d->title);
        int tx = d->x + (d->w - tw) / 2;
        tb_fill(t, tx - 6, d->y + 3, tw + 12, lh + 2, TB_C_BG);
        tb_str(t, tx, tb_text_base(t, d->y + 3, lh + 2), TB_C_FG, d->title);
    }
    tb_draw_kids(t, &d->c);
}

/* Show the dialog modally. Returns the value of the button (or cancel)
 * that closed it, or -1 if the window was closed. The dialog itself is
 * not freed; call tb_dialog_free() when done with it. */
static inline int tb_dialog_run(tb_app *t, tb_dialog *d) {
    tb_dialog *prev = t->modal;
    d->result = -1;
    t->modal = d;
    t->running = 1;
    tb_damage_all(t);
    tb_draw(t);
    while (d->result < 0 && t->running)
        tb_pump(t);
    t->modal = prev;
    tb_damage_all(t);
    return d->result;
}

static inline void tb_dialog_free(tb_dialog *d) {
    for (int i = 0; i < d->c.n; i++)
        d->c.kids[i]->ops->destroy(d->c.kids[i]);
    free(d->c.kids);
    free(d->title);
    free(d);
}

#endif /* TB_DIALOG_H */
