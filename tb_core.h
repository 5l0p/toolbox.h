#ifndef TB_CORE_H
#define TB_CORE_H

/*
 * tb_core.h - shared types, drawing primitives, containers and damage
 * tracking. Header-only: every function is static inline. Include
 * "toolbox.h" rather than this file directly.
 *
 * Coordinates: widgets store their rectangle in the coordinate system of
 * the container that holds them. The window root uses window coordinates.
 * Drawing goes through primitives that add t->ox / t->oy, so a scroll
 * window can draw its children shifted without touching their rectangles.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

/* Timer backend. Linux uses timerfd; OpenBSD (and other BSDs) have no
 * timerfd and use a kqueue with EVFILT_TIMER, which select() can poll
 * the same way. */
#if defined(__linux__)
#  include <sys/timerfd.h>
#  define TB_TIMER_TIMERFD 1
#elif defined(__OpenBSD__)
#  include <sys/types.h>
#  include <sys/time.h>
#  include <sys/event.h>
#  define TB_TIMER_KQUEUE 1
#else
#  error "toolbox: unsupported platform (need Linux or OpenBSD timer support)"
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include "config.h"

/* Upper bound on palette entries; the palette itself sets the real count. */
#define TB_MAX_COLORS 32

/* Colour roles used by the widgets. A config may override any of these;
 * the fallbacks keep the default look. */
#ifndef TB_C_MENUBAR
#define TB_C_MENUBAR      TB_C_BG      /* menubar strip background */
#endif
#ifndef TB_C_STATUS_BG
#define TB_C_STATUS_BG    TB_C_BG      /* status bar background    */
#endif
#ifndef TB_C_STATUS_FG
#define TB_C_STATUS_FG    TB_C_FG      /* status bar text          */
#endif
#ifndef TB_C_SCROLL_THUMB
#define TB_C_SCROLL_THUMB TB_C_BG      /* scroll bar thumb (not while dragged) */
#endif

typedef struct tb_app tb_app;
typedef struct tb_widget tb_widget;
typedef struct tb_dialog tb_dialog;
typedef void (*tb_action)(tb_app *app, void *data);
typedef void (*tb_fd_cb)(tb_app *app, int fd, void *data);

/* A file descriptor the event loop polls (see tb_watch_fd). fd < 0 = free. */
typedef struct tb_watch {
    int fd;
    tb_fd_cb cb;
    void *data;
} tb_watch;

/* Every widget type provides a table of these. Any entry may be NULL.   */
typedef struct tb_wops {
    void (*draw)(tb_app *app, tb_widget *w, int focused);
    int  (*press)(tb_app *app, tb_widget *w, int x, int y); /* 1 = grab pointer */
    void (*motion)(tb_app *app, tb_widget *w, int x, int y);
    void (*release)(tb_app *app, tb_widget *w, int x, int y);
    void (*wheel)(tb_app *app, tb_widget *w, int dir);      /* -1 up, +1 down */
    void (*key)(tb_app *app, tb_widget *w, XKeyEvent *e);
    void (*destroy)(tb_widget *w);                          /* frees w too    */
    void (*overlay)(tb_app *app, tb_widget *w);             /* drawn last while w is app->overlay */
} tb_wops;

/* Base part of every widget. Widget structs embed this as first member. */
struct tb_widget {
    const tb_wops *ops;
    int x, y, w, h;
    int clickable;        /* gets pointer events                    */
    int focusable;        /* becomes focused when pressed           */
    int hidden;           /* not drawn, not hit, not focusable      */
    tb_app *app;          /* set when the widget is added           */
    /* Text insertion (typed-in text from the clipboard), or NULL. */
    void (*text_in)(tb_app *app, tb_widget *w, const char *s, int n);
};

/* A list of child widgets with its own focus. The root window, dialogs and
 * scroll windows each own one. */
typedef struct tb_container {
    tb_widget **kids;
    int n, cap;
    tb_widget *focus;
} tb_container;

struct tb_dialog {
    tb_container c;
    int x, y, w, h;       /* screen rectangle                         */
    char *title;
    int result;           /* -1 while running                         */
    int cancel_value;     /* returned by Escape, -1 = none            */
    tb_widget *def;       /* Return activates this widget, or NULL    */
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
    Pixmap buf;           /* off-screen buffer */
    GC gc;
    XFontStruct *font;
    int fascent, fdescent;
    unsigned long pix[TB_MAX_COLORS];
    Atom wm_delete;
    int width, height;

    tb_menu *menus;
    int nmenus, cmenus;
    char *bar_status;     /* text at the right of the menubar, or NULL */

    tb_container root;    /* top-level widgets */
    tb_container *build;  /* where new widgets go while building, NULL = root */
    tb_dialog *modal;     /* active modal dialog, or NULL */

    int open_menu;        /* -1 = none              */
    int open_item;        /* -1 = none highlighted  */
    tb_widget *grab;      /* receives motion/release while non-NULL */
    tb_widget *overlay;   /* widget whose overlay is drawn on top   */
    unsigned long event_time; /* timestamp of the current press (ms) */
    int ptr_x, ptr_y;     /* pointer position of the latest button event */
    int running;

    /* drawing state */
    int ox, oy;           /* offset added to widget coordinates        */
    XRectangle region;    /* current clip rectangle, device coordinates */
    int dirty, dirty_full;
    int dx0, dy0, dx1, dy1;   /* dirty bounding box (device coords) */

    /* clipboard (see tb_clip.h) */
    char *clip;
    int cliplen;
    Atom a_clip, a_utf8, a_targets, a_string, a_prop;
    tb_widget *paste_target;

    /* window close request (WM_DELETE_WINDOW): return 1 to allow closing */
    int (*close_hook)(tb_app *app, void *data);
    void *close_data;

    /* called after the window is resized (see tb_on_resize) */
    tb_action resize_hook;
    void *resize_data;

    /* fds watched by the event loop (tb_watch_fd / tb_unwatch_fd) */
    tb_watch *watches;
    int nwatches, cwatches;
};

/* Implemented in toolbox.h. Declared here so modal dialogs can run the loop. */
static inline void tb_pump(tb_app *t);
static inline void tb_draw(tb_app *t);

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

/* ---- containers --------------------------------------------------------- */

static inline void tb_container_add(tb_container *c, tb_widget *w) {
    tb_widget **slot = tb_push((void **)&c->kids, &c->n, &c->cap, sizeof *slot);
    *slot = w;
    if (!c->focus && w->focusable && !w->hidden) c->focus = w;
}

/* Widgets constructed while a container is "built" are added to it. */
static inline void tb_add_widget(tb_app *t, tb_widget *w) {
    w->app = t;
    tb_container_add(t->build ? t->build : &t->root, w);
}

/* The container that currently receives input. */
static inline tb_container *tb_active(tb_app *t) {
    return t->modal ? &t->modal->c : &t->root;
}

/* Topmost visible clickable child under (x, y), or NULL. */
static inline tb_widget *tb_hit_in(tb_container *c, int x, int y) {
    for (int i = c->n - 1; i >= 0; i--) {
        tb_widget *w = c->kids[i];
        if (!w->hidden && w->clickable && tb_inside(w, x, y)) return w;
    }
    return NULL;
}

/* ---- rectangles and clip regions ---------------------------------------- */

static inline XRectangle tb_rect(int x, int y, int w, int h) {
    XRectangle r;
    r.x = (short)x;
    r.y = (short)y;
    r.width = (unsigned short)(w < 0 ? 0 : w);
    r.height = (unsigned short)(h < 0 ? 0 : h);
    return r;
}

static inline XRectangle tb_isect(XRectangle a, XRectangle b) {
    int x0 = a.x > b.x ? a.x : b.x;
    int y0 = a.y > b.y ? a.y : b.y;
    int x1 = (a.x + a.width) < (b.x + b.width) ? (a.x + a.width) : (b.x + b.width);
    int y1 = (a.y + a.height) < (b.y + b.height) ? (a.y + a.height) : (b.y + b.height);
    return tb_rect(x0, y0, x1 - x0, y1 - y0);
}

static inline int tb_rect_overlap(XRectangle r, int x, int y, int w, int h) {
    return x < r.x + r.width && x + w > r.x && y < r.y + r.height && y + h > r.y;
}

/* Set the current clip (device coordinates) for everything drawn after. */
static inline void tb_region_set(tb_app *t, XRectangle r) {
    t->region = r;
    XSetClipRectangles(t->dpy, t->gc, 0, 0, &r, 1, Unsorted);
}

/* Clip further drawing to a widget-local rectangle (intersected with the
 * current region). Call tb_clip_reset() afterwards. */
static inline void tb_clip_rect(tb_app *t, int x, int y, int w, int h) {
    XRectangle r = tb_isect(tb_rect(x + t->ox, y + t->oy, w, h), t->region);
    XSetClipRectangles(t->dpy, t->gc, 0, 0, &r, 1, Unsorted);
}

static inline void tb_clip_reset(tb_app *t) {
    XSetClipRectangles(t->dpy, t->gc, 0, 0, &t->region, 1, Unsorted);
}

/* ---- drawing primitives (all go to the off-screen buffer) -------------- */

static inline void tb_set_fg(tb_app *t, int c) {
    XSetForeground(t->dpy, t->gc, t->pix[c]);
}

static inline void tb_fill(tb_app *t, int x, int y, int w, int h, int c) {
    tb_set_fg(t, c);
    XFillRectangle(t->dpy, t->buf, t->gc, x + t->ox, y + t->oy, w, h);
}

static inline void tb_frame(tb_app *t, int x, int y, int w, int h, int c) {
    tb_set_fg(t, c);
    XDrawRectangle(t->dpy, t->buf, t->gc, x + t->ox, y + t->oy, w - 1, h - 1);
}

static inline void tb_draw_line(tb_app *t, int x1, int y1, int x2, int y2, int c) {
    tb_set_fg(t, c);
    XDrawLine(t->dpy, t->buf, t->gc, x1 + t->ox, y1 + t->oy, x2 + t->ox, y2 + t->oy);
}

static inline void tb_hline(tb_app *t, int x1, int x2, int y, int c) {
    tb_draw_line(t, x1, y, x2, y, c);
}

static inline void tb_vline(tb_app *t, int x, int y1, int y2, int c) {
    tb_draw_line(t, x, y1, x, y2, c);
}

/* Full circle or ellipse inside the box (x, y, w, h). */
static inline void tb_arc(tb_app *t, int x, int y, int w, int h, int c, int fill) {
    tb_set_fg(t, c);
    if (fill)
        XFillArc(t->dpy, t->buf, t->gc, x + t->ox, y + t->oy, w, h, 0, 360 * 64);
    else
        XDrawArc(t->dpy, t->buf, t->gc, x + t->ox, y + t->oy, w, h, 0, 360 * 64);
}

/* Filled polygon with at most 12 points, given in widget coordinates. */
static inline void tb_poly(tb_app *t, const XPoint *pts, int n, int c) {
    XPoint p[12];
    if (n > 12) n = 12;
    for (int i = 0; i < n; i++) {
        p[i].x = (short)(pts[i].x + t->ox);
        p[i].y = (short)(pts[i].y + t->oy);
    }
    tb_set_fg(t, c);
    XFillPolygon(t->dpy, t->buf, t->gc, p, n, Convex, CoordModeOrigin);
}

static inline void tb_str(tb_app *t, int x, int base, int c, const char *s) {
    tb_set_fg(t, c);
    XDrawString(t->dpy, t->buf, t->gc, x + t->ox, base + t->oy, s, (int)strlen(s));
}

static inline int tb_text_w(tb_app *t, const char *s) {
    return XTextWidth(t->font, s, (int)strlen(s));
}

/* Baseline y that centres the font vertically inside [y, y+h). */
static inline int tb_text_base(tb_app *t, int y, int h) {
    return y + (h - (t->fascent + t->fdescent)) / 2 + t->fascent;
}

/* Draw every visible child whose rectangle touches the current region. */
static inline void tb_draw_kids(tb_app *t, tb_container *c) {
    for (int i = 0; i < c->n; i++) {
        tb_widget *w = c->kids[i];
        if (w->hidden) continue;
        if (!tb_rect_overlap(t->region, w->x + t->ox, w->y + t->oy, w->w, w->h)) continue;
        w->ops->draw(t, w, w == c->focus);
    }
}

/* ---- damage tracking (partial redraws) ---------------------------------- */

static inline void tb_damage_all(tb_app *t) {
    t->dirty = 1;
    t->dirty_full = 1;
}

static inline void tb_damage_rect(tb_app *t, int x, int y, int w, int h) {
    if (t->dirty_full) return;
    if (!t->dirty) {
        t->dirty = 1;
        t->dx0 = x; t->dy0 = y; t->dx1 = x + w; t->dy1 = y + h;
        return;
    }
    if (x < t->dx0) t->dx0 = x;
    if (y < t->dy0) t->dy0 = y;
    if (x + w > t->dx1) t->dx1 = x + w;
    if (y + h > t->dy1) t->dy1 = y + h;
}

static inline int tb_in_root(const tb_app *t, const tb_widget *w) {
    for (int i = 0; i < t->root.n; i++)
        if (t->root.kids[i] == w) return 1;
    return 0;
}

static inline int tb_in_container(const tb_container *c, const tb_widget *w) {
    for (int i = 0; i < c->n; i++)
        if (c->kids[i] == w) return 1;
    return 0;
}

/* Mark a widget's area as needing a repaint. Direct children of the
 * active container (the open dialog, or the root) repaint only their own
 * rectangle. Anything nested in a scroll window, or drawn as an overlay,
 * causes a full repaint. */
static inline void tb_damage(tb_app *t, tb_widget *w) {
    int direct = t->modal ? tb_in_container(&t->modal->c, w) : tb_in_root(t, w);
    if (!t->ox && !t->oy && direct)
        tb_damage_rect(t, w->x, w->y, w->w, w->h);
    else
        tb_damage_all(t);
}

static inline void tb_widget_hide(tb_widget *w, int hidden) {
    w->hidden = hidden ? 1 : 0;
    if (w->app) tb_damage_all(w->app);
}

#endif /* TB_CORE_H */
