#ifndef TB_TEXTVIEW_H
#define TB_TEXTVIEW_H

/*
 * tb_textview.h - multi-line text view/editor with built-in vertical
 * (line based) and horizontal (pixel based) scrollbars.
 * Click to focus. Typing, Enter, Backspace, Delete, arrows, Home/End,
 * PageUp/PageDown and the mouse wheel are supported. ASCII only.
 */

#include "tb_core.h"
#include "tb_doc.h"
#include "tb_scroll.h"

typedef struct tb_textview {
    tb_widget base;
    tb_doc doc;
    tb_scroll sb[2];      /* [0] vertical (lines), [1] horizontal (pixels) */
    int grab_sb;          /* index of the bar being dragged, or -1         */
} tb_textview;

/* Visible content rectangle: inside the padding, left of / above the bars. */
static inline void tb_tv_content(const tb_widget *w, int *cx, int *cy, int *cw, int *ch) {
    *cx = w->x + TB_TEXT_PAD;
    *cy = w->y + TB_TEXT_PAD;
    *cw = w->w - TB_SCROLL_W - 2 * TB_TEXT_PAD;
    *ch = w->h - TB_SCROLL_W - 2 * TB_TEXT_PAD;
    if (*cw < 1) *cw = 1;
    if (*ch < 1) *ch = 1;
}

static inline int tb_tv_line_h(tb_app *t) {
    int lh = t->fascent + t->fdescent;
    return lh < 1 ? 1 : lh;
}

static inline int tb_tv_maxw(tb_app *t, const tb_doc *d) {
    int m = 0;
    for (int i = 0; i < d->n; i++) {
        int w = XTextWidth(t->font, d->lines[i].s, d->lines[i].len);
        if (w > m) m = w;
    }
    return m;
}

/* Recompute scrollbar ranges from the document and viewport size. */
static inline void tb_tv_sync(tb_app *t, tb_textview *tv) {
    int cx, cy, cw, ch;
    tb_tv_content(&tv->base, &cx, &cy, &cw, &ch);
    tb_sb_set(&tv->sb[0], tv->doc.n, ch / tb_tv_line_h(t));
    tb_sb_set(&tv->sb[1], tb_tv_maxw(t, &tv->doc), cw);
    tv->sb[0].step = 1;
    tv->sb[1].step = TB_SCROLL_STEP;
}

/* Scroll so the cursor is inside the viewport. */
static inline void tb_tv_ensure_cursor(tb_app *t, tb_textview *tv) {
    tb_tv_sync(t, tv);
    tb_scroll *v = &tv->sb[0], *h = &tv->sb[1];
    int row = tv->doc.row;
    if (row < v->value) v->value = row;
    else if (row >= v->value + v->page) v->value = row - v->page + 1;
    tb_sb_clamp(v);

    tb_line *l = &tv->doc.lines[row];
    int px = XTextWidth(t->font, l->s, tv->doc.col);
    if (px < h->value) h->value = px;
    else if (px + TB_SCROLL_STEP > h->value + h->page)
        h->value = px + TB_SCROLL_STEP - h->page;
    tb_sb_clamp(h);
}

/* Put the cursor at the character nearest to pointer position (x, y). */
static inline void tb_tv_click(tb_app *t, tb_textview *tv, int x, int y) {
    int cx, cy, cw, ch;
    tb_tv_content(&tv->base, &cx, &cy, &cw, &ch);
    int row = tv->sb[0].value + (y - cy) / tb_tv_line_h(t);
    if (row < 0) row = 0;
    if (row >= tv->doc.n) row = tv->doc.n - 1;

    tb_line *l = &tv->doc.lines[row];
    int xr = x - cx + tv->sb[1].value;
    int best = 0, bestd = -1;
    for (int c = 0; c <= l->len; c++) {
        int d = XTextWidth(t->font, l->s, c) - xr;
        if (d < 0) d = -d;
        if (bestd < 0 || d < bestd) { bestd = d; best = c; }
    }
    tv->doc.row = row;
    tv->doc.col = best;
}

static inline void tb_tv_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_textview *tv = (tb_textview *)wb;
    tb_tv_sync(t, tv);
    tb_fill(t, wb->x, wb->y, wb->w, wb->h, TB_C_BG);

    int cx, cy, cw, ch;
    tb_tv_content(wb, &cx, &cy, &cw, &ch);
    int lh = tb_tv_line_h(t);
    int top = tv->sb[0].value, page = tv->sb[0].page;
    int hx = tv->sb[1].value;

    XRectangle clip = { (short)cx, (short)cy, (unsigned short)cw, (unsigned short)ch };
    XSetClipRectangles(t->dpy, t->gc, 0, 0, &clip, 1, Unsorted);

    for (int r = top; r < tv->doc.n && r - top <= page; r++) {
        int base = cy + (r - top) * lh + t->fascent;
        tb_str(t, cx - hx, base, TB_C_FG, tv->doc.lines[r].s);
    }
    if (focused) {
        int r = tv->doc.row;
        if (r >= top && r - top <= page) {
            tb_line *l = &tv->doc.lines[r];
            int px = cx - hx + XTextWidth(t->font, l->s, tv->doc.col);
            int y0 = cy + (r - top) * lh;
            tb_vline(t, px, y0, y0 + lh - 1, TB_C_FG);
        }
    }
    XSetClipMask(t->dpy, t->gc, None);

    tb_sb_draw(t, &tv->sb[0]);
    tb_sb_draw(t, &tv->sb[1]);
    tb_fill(t, tv->sb[0].x, tv->sb[1].y, TB_SCROLL_W, TB_SCROLL_W, TB_C_BG);
    tb_frame(t, wb->x, wb->y, wb->w, wb->h, TB_C_FG);
}

static inline int tb_tv_press(tb_app *t, tb_widget *wb, int x, int y) {
    tb_textview *tv = (tb_textview *)wb;
    for (int k = 0; k < 2; k++) {
        if (tb_sb_hit(&tv->sb[k], x, y)) {
            tv->grab_sb = k;
            tb_sb_press(&tv->sb[k], x, y);
            return 1;
        }
    }
    tb_tv_sync(t, tv);
    tb_tv_click(t, tv, x, y);
    tb_tv_ensure_cursor(t, tv);
    return 0;
}

static inline void tb_tv_motion(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t;
    tb_textview *tv = (tb_textview *)wb;
    if (tv->grab_sb >= 0 && tv->sb[tv->grab_sb].dragging)
        tb_sb_drag(&tv->sb[tv->grab_sb], x, y);
}

static inline void tb_tv_release(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t; (void)x; (void)y;
    tb_textview *tv = (tb_textview *)wb;
    if (tv->grab_sb >= 0) tv->sb[tv->grab_sb].dragging = 0;
    tv->grab_sb = -1;
}

static inline void tb_tv_wheel(tb_app *t, tb_widget *wb, int dir) {
    (void)t;
    tb_textview *tv = (tb_textview *)wb;
    tb_sb_adjust(&tv->sb[0], dir * 3 * tv->sb[0].step);
}

static inline void tb_tv_key(tb_app *t, tb_widget *wb, XKeyEvent *e) {
    tb_textview *tv = (tb_textview *)wb;
    KeySym ks;
    char buf[8];
    int n = XLookupString(e, buf, sizeof buf, &ks, NULL);
    tb_doc *d = &tv->doc;
    tb_tv_sync(t, tv);
    int page = tv->sb[0].page;

    switch (ks) {
    case XK_Return:
    case XK_KP_Enter:   tb_doc_newline(d); break;
    case XK_BackSpace:  tb_doc_backspace(d); break;
    case XK_Delete:     tb_doc_delete(d); break;
    case XK_Left:       tb_doc_left(d); break;
    case XK_Right:      tb_doc_right(d); break;
    case XK_Up:         tb_doc_up(d); break;
    case XK_Down:       tb_doc_down(d); break;
    case XK_Home:       d->col = 0; break;
    case XK_End:        d->col = d->lines[d->row].len; break;
    case XK_Prior:      tb_doc_move_rows(d, -page); break;
    case XK_Next:       tb_doc_move_rows(d, page); break;
    default:
        if (n == 1 && buf[0] >= 32 && buf[0] < 127)
            tb_doc_insert_char(d, buf[0]);
        else
            return;
    }
    tb_tv_ensure_cursor(t, tv);
}

static inline void tb_tv_destroy(tb_widget *wb) {
    tb_textview *tv = (tb_textview *)wb;
    tb_doc_free(&tv->doc);
    free(tv);
}

static const tb_wops TB_TEXTVIEW_OPS = {
    tb_tv_draw, tb_tv_press, tb_tv_motion, tb_tv_release,
    tb_tv_wheel, tb_tv_key, tb_tv_destroy
};

static inline tb_textview *tb_textview_new(tb_app *t, int x, int y, int w, int h) {
    tb_textview *tv = calloc(1, sizeof *tv);
    if (!tv) tb_oom();
    tv->base.ops = &TB_TEXTVIEW_OPS;
    tv->base.x = x;
    tv->base.y = y;
    tv->base.w = w;
    tv->base.h = h;
    tv->base.clickable = 1;
    tv->base.focusable = 1;
    tv->grab_sb = -1;

    /* vertical bar on the right, horizontal bar along the bottom */
    tb_scroll *v = &tv->sb[0], *hz = &tv->sb[1];
    v->x = x + w - TB_SCROLL_W;   v->y = y;
    v->w = TB_SCROLL_W;           v->h = h - TB_SCROLL_W;
    v->vertical = 1;
    hz->x = x;                    hz->y = y + h - TB_SCROLL_W;
    hz->w = w - TB_SCROLL_W;      hz->h = TB_SCROLL_W;
    hz->vertical = 0;
    v->total = hz->total = 1;
    v->page = hz->page = 1;

    tb_doc_set(&tv->doc, "");
    tb_add_widget(t, &tv->base);
    if (!t->focus) t->focus = &tv->base;
    return tv;
}

static inline void tb_textview_set(tb_textview *tv, const char *s) {
    tb_doc_set(&tv->doc, s);
    tv->sb[0].value = 0;
    tv->sb[1].value = 0;
}

/* Caller frees the returned string. */
static inline char *tb_textview_get(const tb_textview *tv) {
    return tb_doc_get(&tv->doc);
}

#endif /* TB_TEXTVIEW_H */
