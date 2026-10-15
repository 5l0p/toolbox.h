#ifndef TOOLBOX_H
#define TOOLBOX_H

/*
 * toolbox - a small Macintosh System 6/7 style UI toolkit on plain Xlib
 * (core protocol only). Header-only: include this from exactly the
 * files you need; all functions are static inline.
 *
 * Colours are indices 0..15 into the palette in config.h (TB_* names).
 *
 * Widgets live in their own headers and are included here:
 *   tb_menu.h       menubar (tb_menu_new / tb_menu_item)
 *   tb_label.h      tb_label_new / tb_label_set
 *   tb_button.h     tb_button_new
 *   tb_scrollbar.h  tb_scrollbar_new / _set / _get
 *   tb_textview.h   tb_textview_new / _set / _get
 * Their internals are in tb_core.h, tb_scroll.h and tb_doc.h.
 */

#include "tb_core.h"
#include "tb_doc.h"
#include "tb_scroll.h"
#include "tb_menu.h"
#include "tb_label.h"
#include "tb_button.h"
#include "tb_scrollbar.h"
#include "tb_textview.h"

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

    XMapWindow(t->dpy, t->win);
    XFlush(t->dpy);
    return t;
}

static inline void tb_app_free(tb_app *t) {
    if (!t) return;
    for (int i = 0; i < t->nwidgets; i++)
        t->widgets[i]->ops->destroy(t->widgets[i]);
    free(t->widgets);
    tb_menu_free(t->menus, t->nmenus);
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

/* Topmost clickable widget under (x, y), or NULL. */
static inline tb_widget *tb_hit(tb_app *t, int x, int y) {
    for (int i = t->nwidgets - 1; i >= 0; i--) {
        tb_widget *w = t->widgets[i];
        if (w->clickable && tb_inside(w, x, y)) return w;
    }
    return NULL;
}

/* ---- drawing ----------------------------------------------------------- */

static inline void tb_draw(tb_app *t) {
    tb_fill(t, 0, 0, t->width, t->height, TB_C_BG);
    tb_menu_draw_bar(t);
    for (int i = 0; i < t->nwidgets; i++) {
        tb_widget *w = t->widgets[i];
        w->ops->draw(t, w, w == t->focus);
    }
    tb_menu_draw_dropdown(t);
    XCopyArea(t->dpy, t->buf, t->win, t->gc, 0, 0,
              (unsigned)t->width, (unsigned)t->height, 0, 0);
    XFlush(t->dpy);
}

/* ---- event dispatch ---------------------------------------------------- */

static inline void tb_on_press(tb_app *t, XButtonEvent *e) {
    if (e->button == Button4 || e->button == Button5) {
        tb_widget *w = tb_hit(t, e->x, e->y);
        if (w && w->ops->wheel) w->ops->wheel(t, w, e->button == Button4 ? -1 : 1);
        return;
    }
    if (e->button != Button1) return;
    if (tb_menu_press(t, e->x, e->y)) return;

    tb_widget *w = tb_hit(t, e->x, e->y);
    if (!w) return;
    if (w->focusable) t->focus = w;
    if (w->ops->press && w->ops->press(t, w, e->x, e->y))
        t->grab = w;
}

static inline void tb_on_motion(tb_app *t, XMotionEvent *e) {
    if (t->grab) {
        if (t->grab->ops->motion) t->grab->ops->motion(t, t->grab, e->x, e->y);
        return;
    }
    tb_menu_motion(t, e->x, e->y);
}

static inline void tb_on_release(tb_app *t, XButtonEvent *e) {
    if (e->button != Button1) return;
    if (t->grab) {
        tb_widget *g = t->grab;
        t->grab = NULL;   /* clear first: the callback may change things */
        if (g->ops->release) g->ops->release(t, g, e->x, e->y);
        return;
    }
    tb_menu_release(t, e->x, e->y);
}

static inline void tb_run(tb_app *t) {
    XEvent ev;
    t->running = 1;
    tb_draw(t);
    while (t->running) {
        XNextEvent(t->dpy, &ev);
        switch (ev.type) {
        case Expose:
            if (ev.xexpose.count == 0) tb_draw(t);
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
        case KeyPress: {
            KeySym ks = XLookupKeysym(&ev.xkey, 0);
            if (ks == XK_Escape && t->open_menu >= 0)
                tb_menu_close(t);
            else if (t->focus && t->focus->ops->key)
                t->focus->ops->key(t, t->focus, &ev.xkey);
            break;
        }
        case ClientMessage:
            if ((Atom)ev.xclient.data.l[0] == t->wm_delete)
                t->running = 0;
            break;
        default:
            break;
        }
        if (t->running) tb_draw(t);
    }
}

/* ---- menus (public names) --------------------------------------------- */
/* tb_menu_new() and tb_menu_item() are defined in tb_menu.h. */

#endif /* TOOLBOX_H */
