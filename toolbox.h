#ifndef TOOLBOX_H
#define TOOLBOX_H

/*
 * toolbox - a small Macintosh System 6/7 style UI toolkit on plain Xlib
 * (core protocol plus the ICCCM clipboard). Header-only: include this
 * from the files you need; all functions are static inline.
 *
 * Colours are indices 0..15 into the palette in config.h (TB_* names).
 *
 * Widgets (each in its own header):
 *   tb_menu.h        menubar                   tb_menu_new / tb_menu_item
 *   tb_label.h       static text               tb_label_new / tb_label_set
 *   tb_button.h      push button               tb_button_new
 *   tb_check.h       checkbox                  tb_checkbox_new
 *   tb_radio.h       radio group              tb_radio_new
 *   tb_group.h       group box, rule           tb_groupbox_new, tb_rule_new
 *   tb_icon.h        1-bit bitmap              tb_icon_new
 *   tb_field.h       single-line field         tb_field_new
 *   tb_popup.h       pop-up button             tb_popup_new
 *   tb_combo.h       combo box                 tb_combo_new
 *   tb_scrollbar.h   scroll bar                tb_scrollbar_new
 *   tb_scrollwin.h   scroll window             tb_scrollwin_new
 *   tb_textview.h    multi-line editor         tb_textview_new
 *   tb_view.h        multi-column row engine
 *   tb_list.h        flat list                 tb_list_new
 *   tb_tree.h        tree                      tb_tree_new
 *   tb_slider.h      slider                    tb_slider_new
 *   tb_progress.h    progress bar              tb_progress_new
 *   tb_spin.h        spin box                  tb_spin_new
 *   tb_tabs.h        tab strip                 tb_tabs_new
 *   tb_status.h      status bar                tb_status_new
 *   tb_colorgrid.h   16-colour grid            tb_colorgrid_new
 *   tb_dialog.h      modal dialog              tb_dialog_new / tb_dialog_run
 *   tb_alert.h       alert box                 tb_alert
 *
 * Event loop: tb_run(). Redraws are partial: widgets mark their area dirty
 * (tb_damage) and only that area is repainted.
 */

#include "tb_core.h"
#include "tb_clip.h"
#include "tb_doc.h"
#include "tb_scroll.h"
#include "tb_menu.h"
#include "tb_label.h"
#include "tb_button.h"
#include "tb_scrollbar.h"
#include "tb_textview.h"
#include "tb_view.h"
#include "tb_list.h"
#include "tb_tree.h"
#include "tb_check.h"
#include "tb_radio.h"
#include "tb_group.h"
#include "tb_icon.h"
#include "tb_field.h"
#include "tb_popup.h"
#include "tb_dialog.h"
#include "tb_alert.h"
#include "tb_scrollwin.h"
#include "tb_slider.h"
#include "tb_progress.h"
#include "tb_spin.h"
#include "tb_combo.h"
#include "tb_tabs.h"
#include "tb_status.h"
#include "tb_colorgrid.h"
#include "tb_fs.h"
#include "tb_filedlg.h"

/* ---- application ------------------------------------------------------- */

static inline tb_app *tb_app_new(const char *title, int width, int height) {
    tb_app *t = calloc(1, sizeof *t);
    if (!t) return NULL;
    t->open_menu = -1;
    t->open_item = -1;
    t->width = width;
    t->height = height;

    t->dpy = XOpenDisplay(NULL);
    if (!t->dpy) {
        fprintf(stderr, "toolbox: cannot open display (is DISPLAY set?)\n");
        free(t);
        return NULL;
    }
    t->screen = DefaultScreen(t->dpy);

    /* colours: parse and allocate the 16-entry palette */
    Colormap cm = DefaultColormap(t->dpy, t->screen);
    for (int i = 0; i < 16; i++) {
        XColor c;
        if (!XParseColor(t->dpy, cm, tb_palette[i], &c) ||
            !XAllocColor(t->dpy, cm, &c)) {
            fprintf(stderr, "toolbox: cannot allocate colour %s\n", tb_palette[i]);
            XCloseDisplay(t->dpy);
            free(t);
            return NULL;
        }
        t->pix[i] = c.pixel;
    }

    t->font = XLoadQueryFont(t->dpy, TB_FONT);
    if (!t->font) t->font = XLoadQueryFont(t->dpy, TB_FONT_FALLBACK);
    if (!t->font) {
        fprintf(stderr, "toolbox: cannot load font\n");
        XCloseDisplay(t->dpy);
        free(t);
        return NULL;
    }
    t->fascent = t->font->ascent;
    t->fdescent = t->font->descent;

    t->win = XCreateSimpleWindow(t->dpy, RootWindow(t->dpy, t->screen),
                                 100, 100, (unsigned)width, (unsigned)height, 0,
                                 t->pix[TB_C_FG], t->pix[TB_C_BG]);
    XSelectInput(t->dpy, t->win,
                 ExposureMask | ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask | KeyPressMask | StructureNotifyMask);
    XStoreName(t->dpy, t->win, title);

    XSizeHints sh;
    memset(&sh, 0, sizeof sh);
    sh.flags = PMinSize | PMaxSize;
    sh.min_width = sh.max_width = width;
    sh.min_height = sh.max_height = height;
    XSetWMNormalHints(t->dpy, t->win, &sh);

    t->wm_delete = XInternAtom(t->dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(t->dpy, t->win, &t->wm_delete, 1);

    t->buf = XCreatePixmap(t->dpy, t->win, (unsigned)width, (unsigned)height,
                           (unsigned)DefaultDepth(t->dpy, t->screen));
    t->gc = XCreateGC(t->dpy, t->win, 0, NULL);
    XSetFont(t->dpy, t->gc, t->font->fid);
    tb_region_set(t, tb_rect(0, 0, width, height));

    tb_clip_init(t);
    tb_damage_all(t);

    XMapWindow(t->dpy, t->win);
    XFlush(t->dpy);
    return t;
}

static inline void tb_app_free(tb_app *t) {
    if (!t) return;
    for (int i = 0; i < t->root.n; i++)
        t->root.kids[i]->ops->destroy(t->root.kids[i]);
    free(t->root.kids);
    tb_menu_free(t->menus, t->nmenus);
    free(t->clip);
    XFreeGC(t->dpy, t->gc);
    XFreePixmap(t->dpy, t->buf);
    XFreeFont(t->dpy, t->font);
    XDestroyWindow(t->dpy, t->win);
    XCloseDisplay(t->dpy);
    free(t);
}

static inline void tb_quit(tb_app *t) {
    t->running = 0;
}

/* Ask before the window closes (WM_DELETE_WINDOW). The hook returns 1 to
 * allow it, 0 to keep the window open. */
static inline void tb_on_close(tb_app *t, int (*hook)(tb_app *, void *), void *data) {
    t->close_hook = hook;
    t->close_data = data;
}

static inline void tb_set_title(tb_app *t, const char *title) {
    XStoreName(t->dpy, t->win, title);
}

/* ---- focus -------------------------------------------------------------- */

/* Move focus to the next (dir = 1) or previous (dir = -1) focusable
 * visible child of c, wrapping around. */
static inline void tb_focus_step(tb_app *t, tb_container *c, int dir) {
    (void)t;
    int n = c->n;
    if (n == 0) return;
    int start = -1;
    for (int i = 0; i < n; i++)
        if (c->kids[i] == c->focus) start = i;
    for (int k = 1; k <= n; k++) {
        int base = start < 0 ? (dir > 0 ? -1 : n) : start;
        int i = ((base + dir * k) % n + n) % n;
        tb_widget *w = c->kids[i];
        if (w->focusable && !w->hidden) {
            c->focus = w;
            return;
        }
    }
}

/* ---- drawing ------------------------------------------------------------ */

/* Repaint the dirty area (or everything, if a full repaint is pending). */
static inline void tb_draw(tb_app *t) {
    if (!t->dirty) return;
    XRectangle win = tb_rect(0, 0, t->width, t->height);
    XRectangle r = win;
    if (!t->dirty_full)
        r = tb_isect(win, tb_rect(t->dx0, t->dy0, t->dx1 - t->dx0, t->dy1 - t->dy0));

    t->ox = 0;
    t->oy = 0;
    tb_region_set(t, r);
    tb_fill(t, r.x, r.y, r.width, r.height, TB_C_BG);
    tb_menu_draw_bar(t);
    tb_draw_kids(t, &t->root);
    if (t->modal) tb_dialog_paint(t, t->modal);
    if (t->overlay && t->overlay->ops->overlay)
        t->overlay->ops->overlay(t, t->overlay);
    if (!t->modal) tb_menu_draw_dropdown(t);

    XCopyArea(t->dpy, t->buf, t->win, t->gc, r.x, r.y, r.width, r.height, r.x, r.y);
    XFlush(t->dpy);
    t->dirty = 0;
    t->dirty_full = 0;
}

/* ---- event dispatch ----------------------------------------------------- */

static inline void tb_on_press(tb_app *t, XButtonEvent *e) {
    tb_container *c = tb_active(t);
    if (e->button == Button4 || e->button == Button5) {
        tb_widget *w = tb_hit_in(c, e->x, e->y);
        if (w && w->ops->wheel) {
            w->ops->wheel(t, w, e->button == Button4 ? -1 : 1);
            tb_damage(t, w);
        }
        return;
    }
    if (e->button != Button1) return;
    if (!t->modal && tb_menu_press(t, e->x, e->y)) {
        tb_damage_all(t);
        return;
    }
    tb_widget *w = tb_hit_in(c, e->x, e->y);
    if (!w) return;
    t->event_time = e->time;
    if (w->focusable) c->focus = w;
    if (w->ops->press && w->ops->press(t, w, e->x, e->y))
        t->grab = w;
    tb_damage(t, w);
}

static inline void tb_on_motion(tb_app *t, XMotionEvent *e) {
    if (t->grab) {
        tb_widget *g = t->grab;
        if (g->ops->motion) g->ops->motion(t, g, e->x, e->y);
        tb_damage(t, g);
        return;
    }
    if (!t->modal && tb_menu_motion(t, e->x, e->y))
        tb_damage_all(t);
}

static inline void tb_on_release(tb_app *t, XButtonEvent *e) {
    if (e->button != Button1) return;
    if (t->grab) {
        tb_widget *g = t->grab;
        t->grab = NULL;   /* clear first: the callback may change things */
        if (g->ops->release) g->ops->release(t, g, e->x, e->y);
        tb_damage(t, g);
        return;
    }
    if (!t->modal && tb_menu_release(t, e->x, e->y))
        tb_damage_all(t);
}

static inline void tb_on_key(tb_app *t, XKeyEvent *e) {
    KeySym ks = XLookupKeysym(e, 0);
    tb_container *c = tb_active(t);

    if (ks == XK_Escape) {
        if (t->modal) {
            if (t->modal->cancel_value >= 0)
                tb_dialog_close(t, t->modal, t->modal->cancel_value);
            return;
        }
        if (t->open_menu >= 0) {
            tb_menu_close(t);
            tb_damage_all(t);
            return;
        }
    }
    if (ks == XK_Tab || ks == XK_ISO_Left_Tab) {
        tb_focus_step(t, c, (e->state & ShiftMask) ? -1 : 1);
        tb_damage_all(t);
        return;
    }

    tb_widget *f = c->focus;
    /* Return activates the dialog's default button, unless the focused
     * widget uses Return for itself. */
    if (t->modal && t->modal->def && (ks == XK_Return || ks == XK_KP_Enter) &&
        (!f || (f->ops != &TB_TEXTVIEW_OPS && f->ops != &TB_FIELD_OPS &&
                f->ops != &TB_VIEW_OPS))) {
        tb_widget *d = t->modal->def;
        if (d->ops->key) d->ops->key(t, d, e);
        return;
    }
    if (f && f->ops->key) {
        f->ops->key(t, f, e);
        tb_damage(t, f);
    }
}

/* Process one X event and repaint. Modal dialogs call this in a loop. */
static inline void tb_pump(tb_app *t) {
    XEvent ev;
    int was = t->overlay != NULL || t->open_menu >= 0 || t->modal != NULL;
    XNextEvent(t->dpy, &ev);
    switch (ev.type) {
    case Expose:
        if (ev.xexpose.count == 0) tb_damage_all(t);
        break;
    case ButtonPress:
        tb_on_press(t, &ev.xbutton);
        break;
    case MotionNotify:
        tb_on_motion(t, &ev.xmotion);
        break;
    case ButtonRelease:
        tb_on_release(t, &ev.xbutton);
        break;
    case KeyPress:
        tb_on_key(t, &ev.xkey);
        break;
    case SelectionRequest:
        tb_clip_on_request(t, &ev.xselectionrequest);
        break;
    case SelectionNotify:
        tb_clip_on_notify(t, &ev.xselection);
        break;
    case ClientMessage:
        if ((Atom)ev.xclient.data.l[0] == t->wm_delete) {
            if (!t->close_hook || t->close_hook(t, t->close_data))
                t->running = 0;
        }
        break;
    default:
        break;
    }
    /* Menus, popups and dialogs paint over the window: repaint it all
     * when any of them was or is open. */
    if (was || t->overlay || t->open_menu >= 0 || t->modal)
        tb_damage_all(t);
    tb_draw(t);
}

static inline void tb_run(tb_app *t) {
    t->running = 1;
    tb_damage_all(t);
    while (t->running)
        tb_pump(t);
}

#endif /* TOOLBOX_H */
