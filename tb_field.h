#ifndef TB_FIELD_H
#define TB_FIELD_H

/*
 * tb_field.h - single-line text field with a caret, selection, horizontal
 * scrolling and the X clipboard.
 *
 *   click / drag   place caret, select by dragging
 *   Left/Right     move (Shift extends the selection)
 *   Home/End       start / end of line
 *   BackSpace/Del  delete selection or character
 *   Ctrl-A/X/C/V   select all / cut / copy / paste (CLIPBOARD)
 *   Return         calls the accept callback
 *
 * The field owns its text. tb_field_get() returns a pointer that is valid
 * until the next edit. Pasted text is filtered to printable ASCII.
 */

#include "tb_core.h"
#include "tb_clip.h"

#define TB_FIELD_PAD 3

typedef struct tb_field {
    tb_widget base;
    char *s;
    int len, cap;
    int cur, anchor;     /* selection is [min, max); cur == anchor means none */
    int hx;              /* horizontal scroll, pixels                         */
    int dragging;
    int rightpad;        /* width reserved on the right (used by combo boxes)  */
    int plain;           /* 1 = no frame around the field                      */
    tb_action on_accept, on_change;
    void *accept_data, *change_data;
} tb_field;

static inline void tb_field_reserve(tb_field *f, int need) {
    if (need <= f->cap) return;
    int nc = f->cap ? f->cap : 32;
    while (nc < need) nc *= 2;
    char *p = realloc(f->s, (size_t)nc);
    if (!p) tb_oom();
    f->s = p;
    f->cap = nc;
}

static inline int tb_field_has_sel(const tb_field *f) {
    return f->cur != f->anchor;
}

static inline void tb_field_range(const tb_field *f, int *a, int *b) {
    *a = f->cur < f->anchor ? f->cur : f->anchor;
    *b = f->cur > f->anchor ? f->cur : f->anchor;
}

/* Remove bytes [a, b) and put the caret at a. */
static inline void tb_field_erase(tb_field *f, int a, int b) {
    memmove(f->s + a, f->s + b, (size_t)(f->len - b + 1));
    f->len -= b - a;
    f->cur = f->anchor = a;
}

static inline void tb_field_delete_sel(tb_field *f) {
    if (!tb_field_has_sel(f)) return;
    int a, b;
    tb_field_range(f, &a, &b);
    tb_field_erase(f, a, b);
}

static inline void tb_field_insert(tb_field *f, const char *src, int n) {
    tb_field_delete_sel(f);
    tb_field_reserve(f, f->len + n + 1);
    memmove(f->s + f->cur + n, f->s + f->cur, (size_t)(f->len - f->cur + 1));
    memcpy(f->s + f->cur, src, (size_t)n);
    f->len += n;
    f->cur += n;
    f->anchor = f->cur;
}

static inline int tb_field_inner_w(const tb_field *f) {
    int w = f->base.w - f->rightpad - 2 * TB_FIELD_PAD - 2;
    return w < 1 ? 1 : w;
}

/* Keep the caret visible and the scroll offset sensible. */
static inline void tb_field_visible(tb_app *t, tb_field *f) {
    int iw = tb_field_inner_w(f);
    int total = XTextWidth(t->font, f->s, f->len);
    int px = XTextWidth(t->font, f->s, f->cur);
    if (total <= iw) f->hx = 0;
    else {
        if (px - f->hx < 0) f->hx = px;
        if (px - f->hx > iw - 2) f->hx = px - iw + 2;
        if (f->hx > total - iw + 2) f->hx = total - iw + 2;
        if (f->hx < 0) f->hx = 0;
    }
}

static inline int tb_field_index_at(tb_app *t, const tb_field *f, int x) {
    int ix = f->base.x + TB_FIELD_PAD + 1;
    int xr = x - ix + f->hx;
    int best = 0, bestd = -1;
    for (int c = 0; c <= f->len; c++) {
        int d = XTextWidth(t->font, f->s, c) - xr;
        if (d < 0) d = -d;
        if (bestd < 0 || d < bestd) { bestd = d; best = c; }
    }
    return best;
}

static inline void tb_field_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_field *f = (tb_field *)wb;
    tb_field_visible(t, f);
    int fw = wb->w - f->rightpad;

    tb_fill(t, wb->x, wb->y, fw, wb->h, TB_C_BG);
    if (!f->plain) tb_frame(t, wb->x, wb->y, fw, wb->h, TB_C_FG);

    int ix = wb->x + TB_FIELD_PAD + 1, iy = wb->y + 2;
    int iw = tb_field_inner_w(f), ih = wb->h - 4;
    int base = tb_text_base(t, wb->y, wb->h);

    tb_clip_rect(t, ix, iy, iw, ih);
    tb_str(t, ix - f->hx, base, TB_C_FG, f->s);

    if (tb_field_has_sel(f)) {
        int a, b;
        tb_field_range(f, &a, &b);
        int x1 = ix - f->hx + XTextWidth(t->font, f->s, a);
        int x2 = ix - f->hx + XTextWidth(t->font, f->s, b);
        int cx1 = x1 > ix ? x1 : ix;
        int cx2 = x2 < ix + iw ? x2 : ix + iw;
        if (cx2 > cx1) {
            tb_clip_rect(t, cx1, iy, cx2 - cx1, ih);
            tb_fill(t, cx1, iy, cx2 - cx1, ih, TB_C_SEL_BG);
            tb_str(t, ix - f->hx, base, TB_C_SEL_FG, f->s);
            tb_clip_rect(t, ix, iy, iw, ih);
        }
    } else if (focused) {
        int cx = ix - f->hx + XTextWidth(t->font, f->s, f->cur);
        tb_vline(t, cx, iy + 1, iy + ih - 2, TB_C_FG);
    }
    tb_clip_reset(t);
}

static inline void tb_field_changed(tb_app *t, tb_field *f) {
    tb_field_visible(t, f);
    if (f->on_change) f->on_change(t, f->change_data);
}

static inline int tb_field_press(tb_app *t, tb_widget *wb, int x, int y) {
    (void)y;
    tb_field *f = (tb_field *)wb;
    f->cur = f->anchor = tb_field_index_at(t, f, x);
    f->dragging = 1;
    tb_field_visible(t, f);
    return 1;   /* grab so a drag selects */
}

static inline void tb_field_motion(tb_app *t, tb_widget *wb, int x, int y) {
    (void)y;
    tb_field *f = (tb_field *)wb;
    if (!f->dragging) return;
    f->cur = tb_field_index_at(t, f, x);
    tb_field_visible(t, f);
}

static inline void tb_field_release(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t; (void)x; (void)y;
    ((tb_field *)wb)->dragging = 0;
}

/* Text arriving from the clipboard: keep printable ASCII only. */
static inline void tb_field_text_in(tb_app *t, tb_widget *wb, const char *s, int n) {
    tb_field *f = (tb_field *)wb;
    char buf[256];
    int m = 0;
    for (int i = 0; i < n && m < (int)sizeof buf; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 32 && c < 127) buf[m++] = (char)c;
        else if (c >= 128) buf[m++] = '?';
        /* control characters (newlines, tabs) are dropped */
    }
    if (m > 0) {
        tb_field_insert(f, buf, m);
        tb_field_changed(t, f);
        tb_damage(t, wb);
    }
}

static inline void tb_field_key(tb_app *t, tb_widget *wb, XKeyEvent *e) {
    tb_field *f = (tb_field *)wb;
    KeySym ks;
    char buf[16];
    int n = XLookupString(e, buf, sizeof buf, &ks, NULL);
    int ctrl = (e->state & ControlMask) != 0;
    int shift = (e->state & ShiftMask) != 0;
    int changed = 0;

    switch (ks) {
    case XK_Left:
    case XK_Right:
    case XK_Home:
    case XK_End: {
        int target;
        if (!shift && tb_field_has_sel(f) && (ks == XK_Left || ks == XK_Right)) {
            int a, b;
            tb_field_range(f, &a, &b);
            target = (ks == XK_Left) ? a : b;
        } else if (ks == XK_Left) {
            target = f->cur > 0 ? f->cur - 1 : 0;
        } else if (ks == XK_Right) {
            target = f->cur < f->len ? f->cur + 1 : f->len;
        } else if (ks == XK_Home) {
            target = 0;
        } else {
            target = f->len;
        }
        f->cur = target;
        if (!shift) f->anchor = target;
        break;
    }
    case XK_BackSpace:
        if (tb_field_has_sel(f)) { tb_field_delete_sel(f); changed = 1; }
        else if (f->cur > 0) { tb_field_erase(f, f->cur - 1, f->cur); changed = 1; }
        break;
    case XK_Delete:
        if (tb_field_has_sel(f)) { tb_field_delete_sel(f); changed = 1; }
        else if (f->cur < f->len) { tb_field_erase(f, f->cur, f->cur + 1); changed = 1; }
        break;
    case XK_Return:
    case XK_KP_Enter:
        if (f->on_accept) f->on_accept(t, f->accept_data);
        return;
    default:
        if (ctrl) {
            if (ks == XK_a || ks == XK_A) {
                f->anchor = 0;
                f->cur = f->len;
            } else if (ks == XK_c || ks == XK_C || ks == XK_x || ks == XK_X) {
                if (tb_field_has_sel(f)) {
                    int a, b;
                    tb_field_range(f, &a, &b);
                    tb_clip_copy(t, f->s + a, b - a);
                    if (ks == XK_x || ks == XK_X) { tb_field_delete_sel(f); changed = 1; }
                }
            } else if (ks == XK_v || ks == XK_V) {
                /* may deliver synchronously, or later via text_in */
                tb_clip_paste(t);
                return;
            } else {
                return;
            }
        } else if (n == 1 && buf[0] >= 32 && buf[0] < 127) {
            tb_field_insert(f, buf, 1);
            changed = 1;
        } else {
            return;
        }
        break;
    }

    if (changed) tb_field_changed(t, f);
    else tb_field_visible(t, f);
}

static inline void tb_field_destroy(tb_widget *wb) {
    tb_field *f = (tb_field *)wb;
    free(f->s);
    free(f);
}

static const tb_wops TB_FIELD_OPS = {
    tb_field_draw, tb_field_press, tb_field_motion, tb_field_release,
    NULL, tb_field_key, tb_field_destroy, NULL
};

static inline tb_field *tb_field_new(tb_app *t, int x, int y, int w, int h) {
    tb_field *f = calloc(1, sizeof *f);
    if (!f) tb_oom();
    f->base.ops = &TB_FIELD_OPS;
    f->base.x = x;
    f->base.y = y;
    f->base.w = w;
    f->base.h = h;
    f->base.clickable = 1;
    f->base.focusable = 1;
    f->base.text_in = tb_field_text_in;
    f->cap = 32;
    f->s = malloc((size_t)f->cap);
    if (!f->s) tb_oom();
    f->s[0] = 0;
    tb_add_widget(t, &f->base);
    return f;
}

/* Replace the text; the caret goes to the end and nothing is selected. */
static inline void tb_field_set(tb_field *f, const char *s) {
    int n = (int)strlen(s);
    tb_field_reserve(f, n + 1);
    memcpy(f->s, s, (size_t)n + 1);
    f->len = n;
    f->cur = f->anchor = n;
    if (f->base.app) tb_damage(f->base.app, &f->base);
}

static inline const char *tb_field_get(const tb_field *f) {
    return f->s;
}

static inline void tb_field_on_accept(tb_field *f, tb_action cb, void *data) {
    f->on_accept = cb;
    f->accept_data = data;
}

static inline void tb_field_on_change(tb_field *f, tb_action cb, void *data) {
    f->on_change = cb;
    f->change_data = data;
}

#endif /* TB_FIELD_H */
