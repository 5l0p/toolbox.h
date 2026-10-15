#ifndef TB_CORE_H
#define TB_CORE_H

/*
 * tb_core.h - shared types, drawing primitives and the widget registry.
 * Header-only: every function is static inline. Include "toolbox.h"
 * rather than this file directly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include "config.h"

typedef struct tb_app tb_app;
typedef struct tb_widget tb_widget;
typedef void (*tb_action)(tb_app *app, void *data);

/* Every widget type provides a table of these. Any entry may be NULL.   */
typedef struct tb_wops {
    void (*draw)(tb_app *app, tb_widget *w, int focused);
    int  (*press)(tb_app *app, tb_widget *w, int x, int y); /* 1 = grab pointer */
    void (*motion)(tb_app *app, tb_widget *w, int x, int y);
    void (*release)(tb_app *app, tb_widget *w, int x, int y);
    void (*wheel)(tb_app *app, tb_widget *w, int dir);      /* -1 up, +1 down */
    void (*key)(tb_app *app, tb_widget *w, XKeyEvent *e);
    void (*destroy)(tb_widget *w);                          /* frees w too    */
} tb_wops;

/* Base part of every widget. Widget structs embed this as first member. */
struct tb_widget {
    const tb_wops *ops;
    int x, y, w, h;
    int clickable;        /* gets pointer events                    */
    int focusable;        /* becomes focused when pressed           */
};

typedef struct {
    char *title;          /* NULL for separator */
    tb_action cb;
    void *data;
} tb_item;

typedef struct {
    char *title;
    int x, tw;            /* title cell in the menubar */
    int dw;               /* dropdown width            */
    tb_item *items;
    int nitems, citems;
} tb_menu;

struct tb_app {
    Display *dpy;
    int screen;
    Window win;
    Pixmap buf;           /* off-screen buffer, avoids flicker */
    GC gc;
    XFontStruct *font;
    int fascent, fdescent;
    unsigned long pix[16];
    Atom wm_delete;
    int width, height;

    tb_menu *menus;
    int nmenus, cmenus;
    tb_widget **widgets;
    int nwidgets, cwidgets;

    int open_menu;        /* -1 = none              */
    int open_item;        /* -1 = none highlighted  */
    tb_widget *grab;      /* receives motion/release while non-NULL */
    tb_widget *focus;     /* receives key events                    */
    int running;
};

/* ---- utilities --------------------------------------------------------- */

static inline void tb_oom(void) {
    fprintf(stderr, "toolbox: out of memory\n");
    exit(1);
}

static inline char *tb_strdup(const char *s) {
    char *d = malloc(strlen(s) + 1);
    if (!d) tb_oom();
    strcpy(d, s);
    return d;
}

/* Append one zeroed element to a growable array; returns pointer to it. */
static inline void *tb_push(void **arr, int *n, int *cap, size_t esz) {
    if (*n == *cap) {
        int nc = *cap ? *cap * 2 : 8;
        void *na = realloc(*arr, (size_t)nc * esz);
        if (!na) tb_oom();
        *arr = na;
        *cap = nc;
    }
    void *p = (char *)*arr + (size_t)(*n) * esz;
    (*n)++;
    memset(p, 0, esz);
    return p;
}

static inline int tb_inside(const tb_widget *w, int x, int y) {
    return x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + w->h;
}

static inline void tb_add_widget(tb_app *t, tb_widget *w) {
    tb_widget **slot = tb_push((void **)&t->widgets, &t->nwidgets,
                               &t->cwidgets, sizeof *slot);
    *slot = w;
}

/* ---- drawing primitives (all go to the off-screen buffer) -------------- */

static inline void tb_set_fg(tb_app *t, int c) {
    XSetForeground(t->dpy, t->gc, t->pix[c]);
}

static inline void tb_fill(tb_app *t, int x, int y, int w, int h, int c) {
    tb_set_fg(t, c);
    XFillRectangle(t->dpy, t->buf, t->gc, x, y, w, h);
}

static inline void tb_frame(tb_app *t, int x, int y, int w, int h, int c) {
    tb_set_fg(t, c);
    XDrawRectangle(t->dpy, t->buf, t->gc, x, y, w - 1, h - 1);
}

static inline void tb_hline(tb_app *t, int x1, int x2, int y, int c) {
    tb_set_fg(t, c);
    XDrawLine(t->dpy, t->buf, t->gc, x1, y, x2, y);
}

static inline void tb_vline(tb_app *t, int x, int y1, int y2, int c) {
    tb_set_fg(t, c);
    XDrawLine(t->dpy, t->buf, t->gc, x, y1, x, y2);
}

static inline void tb_str(tb_app *t, int x, int base, int c, const char *s) {
    tb_set_fg(t, c);
    XDrawString(t->dpy, t->buf, t->gc, x, base, s, (int)strlen(s));
}

static inline int tb_text_w(tb_app *t, const char *s) {
    return XTextWidth(t->font, s, (int)strlen(s));
}

/* Baseline y that centres the font vertically inside [y, y+h). */
static inline int tb_text_base(tb_app *t, int y, int h) {
    return y + (h - (t->fascent + t->fdescent)) / 2 + t->fascent;
}

#endif /* TB_CORE_H */
