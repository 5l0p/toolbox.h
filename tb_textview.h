#ifndef TB_TEXTVIEW_H
#define TB_TEXTVIEW_H

/*
 * tb_textview.h - multi-line text editor with vertical (line based) and
 * horizontal (pixel based) scroll bars, a selection and the X clipboard.
 *
 *   click / drag            place the caret, select by dragging
 *   Shift + movement        extend the selection
 *   arrows, Home/End,       move (PageUp/PageDown move a page), wheel scrolls
 *   typing, Return,         edit; typing and paste replace the selection
 *   BackSpace, Delete
 *   Ctrl-A / X / C / V      select all / cut / copy / paste (CLIPBOARD)
 *
 * ASCII only (columns are bytes). Tab moves focus, so tabs cannot be typed.
 * tb_textview_on_update() is called after every edit, click and caret move.
 */

#include "tb_core.h"
#include "tb_doc.h"
#include "tb_scroll.h"
#include "tb_clip.h"

typedef struct tb_textview {
    tb_widget base;
    tb_doc doc;
    tb_scroll sb[2];      /* [0] vertical (lines), [1] horizontal (pixels) */
    int grab_sb;          /* index of the bar being dragged, or -1         */
    int ar, ac;           /* selection anchor; equal to the caret = none   */
    int dragging;         /* selecting with the mouse                      */
    int modified;         /* edited since the last tb_textview_set()       */
    tb_action on_update;
    void *update_data;
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

/* Scroll so the caret is inside the viewport. */
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

/* Put the caret at the character nearest to pointer position (x, y). */
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

/* ---- selection --------------------------------------------------------- */

static inline int tb_tv_has_sel(const tb_textview *tv) {
    return tv->ar != tv->doc.row || tv->ac != tv->doc.col;
}

/* Selection as an ordered range (r1,c1) .. (r2,c2). */
static inline void tb_tv_sel_range(const tb_textview *tv, int *r1, int *c1, int *r2, int *c2) {
    int ar = tv->ar, ac = tv->ac, cr = tv->doc.row, cc = tv->doc.col;
    if (ar < cr || (ar == cr && ac <= cc)) { *r1 = ar; *c1 = ac; *r2 = cr; *c2 = cc; }
    else                                   { *r1 = cr; *c1 = cc; *r2 = ar; *c2 = ac; }
}

static inline void tb_tv_collapse(tb_textview *tv) {
    tv->ar = tv->doc.row;
    tv->ac = tv->doc.col;
}

/* Delete the selected text. Returns 1 if anything was deleted. */
static inline int tb_tv_delete_sel(tb_textview *tv) {
    if (!tb_tv_has_sel(tv)) return 0;
    int r1, c1, r2, c2;
    tb_tv_sel_range(tv, &r1, &c1, &r2, &c2);
    tb_doc_delete_range(&tv->doc, r1, c1, r2, c2);
    tb_tv_collapse(tv);
    return 1;
}

static inline void tb_tv_notify(tb_app *t, tb_textview *tv) {
    if (tv->on_update) tv->on_update(t, tv->update_data);
}

/* ---- drawing ----------------------------------------------------------- */

static inline void tb_tv_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_textview *tv = (tb_textview *)wb;
    tb_tv_sync(t, tv);
    tb_fill(t, wb->x, wb->y, wb->w, wb->h, TB_C_BG);

    int cx, cy, cw, ch;
    tb_tv_content(wb, &cx, &cy, &cw, &ch);
    int lh = tb_tv_line_h(t);
    int top = tv->sb[0].value, page = tv->sb[0].page;
    int hx = tv->sb[1].value;
    int sel = tb_tv_has_sel(tv);
    int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
    if (sel) tb_tv_sel_range(tv, &r1, &c1, &r2, &c2);

    tb_clip_rect(t, cx, cy, cw, ch);

    for (int r = top; r < tv->doc.n && r - top <= page; r++) {
        int base = cy + (r - top) * lh + t->fascent;
        tb_str(t, cx - hx, base, TB_C_FG, tv->doc.lines[r].s);
    }

    /* selected parts of the visible lines: inverted background and text */
    if (sel) {
        for (int r = top; r < tv->doc.n && r - top <= page; r++) {
            if (r < r1 || r > r2) continue;
            tb_line *l = &tv->doc.lines[r];
            int sc = r == r1 ? c1 : 0;
            int ec = r == r2 ? c2 : l->len;
            int x1 = cx - hx + XTextWidth(t->font, l->s, sc);
            int x2 = cx - hx + XTextWidth(t->font, l->s, ec);
            /* a selected line break shows as a small extra block */
            if (r != r2) x2 += t->fascent / 2 + 2;
            int y0 = cy + (r - top) * lh;
            if (x2 <= x1) continue;
            tb_clip_rect(t, x1, y0, x2 - x1, lh);
            tb_fill(t, x1, y0, x2 - x1, lh, TB_C_SEL_BG);
            tb_str(t, cx - hx, y0 + t->fascent, TB_C_SEL_FG, l->s);
            tb_clip_rect(t, cx, cy, cw, ch);
        }
    }

    if (focused && !sel) {
        int r = tv->doc.row;
        if (r >= top && r - top <= page) {
            tb_line *l = &tv->doc.lines[r];
            int px = cx - hx + XTextWidth(t->font, l->s, tv->doc.col);
            int y0 = cy + (r - top) * lh;
            tb_vline(t, px, y0, y0 + lh - 1, TB_C_FG);
        }
    }
    tb_clip_reset(t);

    tb_sb_draw(t, &tv->sb[0]);
    tb_sb_draw(t, &tv->sb[1]);
    tb_fill(t, tv->sb[0].x, tv->sb[1].y, TB_SCROLL_W, TB_SCROLL_W, TB_C_BG);
    tb_frame(t, wb->x, wb->y, wb->w, wb->h, TB_C_FG);
}

/* ---- mouse ------------------------------------------------------------- */

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
    tb_tv_collapse(tv);
    tv->dragging = 1;
    tb_tv_ensure_cursor(t, tv);
    tb_tv_notify(t, tv);
    return 1;             /* grab: a drag extends the selection */
}

static inline void tb_tv_motion(tb_app *t, tb_widget *wb, int x, int y) {
    tb_textview *tv = (tb_textview *)wb;
    if (tv->grab_sb >= 0) {
        if (tv->sb[tv->grab_sb].dragging) tb_sb_drag(&tv->sb[tv->grab_sb], x, y);
        return;
    }
    if (tv->dragging) {
        tb_tv_click(t, tv, x, y);
        tb_tv_ensure_cursor(t, tv);
        tb_tv_notify(t, tv);
    }
}

static inline void tb_tv_release(tb_app *t, tb_widget *wb, int x, int y) {
    (void)x; (void)y;
    tb_textview *tv = (tb_textview *)wb;
    if (tv->grab_sb >= 0) tv->sb[tv->grab_sb].dragging = 0;
    tv->grab_sb = -1;
    tv->dragging = 0;
    tb_tv_notify(t, tv);
}

static inline void tb_tv_wheel(tb_app *t, tb_widget *wb, int dir) {
    (void)t;
    tb_textview *tv = (tb_textview *)wb;
    tb_sb_adjust(&tv->sb[0], dir * 3 * tv->sb[0].step);
}

/* ---- editing commands (used by keys and the Edit menu) ----------------- */

static inline void tb_textview_select_all(tb_app *t, tb_textview *tv) {
    tv->doc.row = tv->doc.n - 1;
    tv->doc.col = tv->doc.lines[tv->doc.row].len;
    tv->ar = 0;
    tv->ac = 0;
    tb_tv_ensure_cursor(t, tv);
    tb_tv_notify(t, tv);
}

static inline void tb_textview_copy(tb_app *t, tb_textview *tv) {
    if (!tb_tv_has_sel(tv)) return;
    int r1, c1, r2, c2;
    tb_tv_sel_range(tv, &r1, &c1, &r2, &c2);
    char *s = tb_doc_range_text(&tv->doc, r1, c1, r2, c2);
    tb_clip_copy(t, s, (int)strlen(s));
    free(s);
}

static inline void tb_textview_cut(tb_app *t, tb_textview *tv) {
    if (!tb_tv_has_sel(tv)) return;
    tb_textview_copy(t, tv);
    tb_tv_delete_sel(tv);
    tv->modified = 1;
    tb_tv_ensure_cursor(t, tv);
    tb_tv_notify(t, tv);
}

static inline void tb_tv_key(tb_app *t, tb_widget *wb, XKeyEvent *e) {
    tb_textview *tv = (tb_textview *)wb;
    KeySym ks;
    char buf[8];
    int n = XLookupString(e, buf, sizeof buf, &ks, NULL);
    int ctrl = (e->state & ControlMask) != 0;
    int shift = (e->state & ShiftMask) != 0;
    tb_doc *d = &tv->doc;
    tb_tv_sync(t, tv);
    int page = tv->sb[0].page;

    if (ctrl) {
        switch (ks) {
        case XK_a: case XK_A: tb_textview_select_all(t, tv); break;
        case XK_c: case XK_C: tb_textview_copy(t, tv); break;
        case XK_x: case XK_X: tb_textview_cut(t, tv); break;
        case XK_v: case XK_V: tb_clip_paste(t); break;   /* delivers via text_in */
        default: break;
        }
        return;
    }

    int edited = 0;
    switch (ks) {
    case XK_Return:
    case XK_KP_Enter:
        tb_tv_delete_sel(tv);
        tb_doc_newline(d);
        edited = 1;
        break;
    case XK_BackSpace:
        if (!tb_tv_delete_sel(tv)) tb_doc_backspace(d);
        edited = 1;
        break;
    case XK_Delete:
        if (!tb_tv_delete_sel(tv)) tb_doc_delete(d);
        edited = 1;
        break;
    case XK_Left:
    case XK_Right:
    case XK_Up:
    case XK_Down:
    case XK_Home:
    case XK_End:
    case XK_Prior:
    case XK_Next:
        if (!shift && tb_tv_has_sel(tv) && (ks == XK_Left || ks == XK_Right)) {
            /* plain Left/Right with a selection goes to its edge */
            int r1, c1, r2, c2;
            tb_tv_sel_range(tv, &r1, &c1, &r2, &c2);
            if (ks == XK_Left) { d->row = r1; d->col = c1; }
            else               { d->row = r2; d->col = c2; }
        } else {
            switch (ks) {
            case XK_Left:  tb_doc_left(d); break;
            case XK_Right: tb_doc_right(d); break;
            case XK_Up:    tb_doc_up(d); break;
            case XK_Down:  tb_doc_down(d); break;
            case XK_Home:  d->col = 0; break;
            case XK_End:   d->col = d->lines[d->row].len; break;
            case XK_Prior: tb_doc_move_rows(d, -page); break;
            default:       tb_doc_move_rows(d, page); break;   /* XK_Next */
            }
        }
        if (!shift) tb_tv_collapse(tv);
        break;
    default:
        if (n == 1 && buf[0] >= 32 && buf[0] < 127) {
            tb_tv_delete_sel(tv);
            tb_doc_insert_char(d, buf[0]);
            edited = 1;
            break;
        }
        return;
    }

    if (edited) {
        tv->modified = 1;
        tb_tv_collapse(tv);
    }
    tb_tv_ensure_cursor(t, tv);
    tb_tv_notify(t, tv);
}

/* Pasted text: the selection is replaced; newlines split lines. */
static inline void tb_tv_text_in(tb_app *t, tb_widget *wb, const char *s, int n) {
    tb_textview *tv = (tb_textview *)wb;
    tb_tv_delete_sel(tv);
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\n' || c == '\r') tb_doc_newline(&tv->doc);
        else if (c >= 32 && c < 127) tb_doc_insert_char(&tv->doc, (char)c);
    }
    tb_tv_collapse(tv);
    tv->modified = 1;
    tb_tv_ensure_cursor(t, tv);
    tb_tv_notify(t, tv);
    tb_damage(t, wb);
}

static inline void tb_tv_destroy(tb_widget *wb) {
    tb_textview *tv = (tb_textview *)wb;
    tb_doc_free(&tv->doc);
    free(tv);
}

static const tb_wops TB_TEXTVIEW_OPS = {
    tb_tv_draw, tb_tv_press, tb_tv_motion, tb_tv_release,
    tb_tv_wheel, tb_tv_key, tb_tv_destroy, NULL
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
    tv->base.text_in = tb_tv_text_in;
    tb_add_widget(t, &tv->base);
    return tv;
}

/* Replace the text. Clears the edited flag and the selection. */
static inline void tb_textview_set(tb_textview *tv, const char *s) {
    tb_doc_set(&tv->doc, s);
    tv->ar = tv->ac = 0;
    tv->modified = 0;
    tv->sb[0].value = 0;
    tv->sb[1].value = 0;
    if (tv->base.app) tb_damage(tv->base.app, &tv->base);
}

/* Caller frees the returned string. */
static inline char *tb_textview_get(const tb_textview *tv) {
    return tb_doc_get(&tv->doc);
}

static inline int tb_textview_modified(const tb_textview *tv) {
    return tv->modified;
}

static inline void tb_textview_clear_modified(tb_textview *tv) {
    tv->modified = 0;
}

/* Caret position, 1-based (line and column). */
static inline void tb_textview_cursor(const tb_textview *tv, int *line, int *col) {
    *line = tv->doc.row + 1;
    *col = tv->doc.col + 1;
}

/* Called after every edit, click and caret move. */
static inline void tb_textview_on_update(tb_textview *tv, tb_action cb, void *data) {
    tv->on_update = cb;
    tv->update_data = data;
}

#endif /* TB_TEXTVIEW_H */
