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
 * ASCII only (columns are bytes). Tab inserts a tab character, shown
 * up to the next TB_TAB_WIDTH (4) column stop. Shift+Tab moves focus.
 * tb_textview_on_update() is called after every edit, click and caret move.
 */

#include "tb_core.h"
#include "tb_doc.h"
#include "tb_scroll.h"
#include "tb_clip.h"

typedef struct {
    int r, c0, c1;        /* visual row: logical line r, bytes [c0, c1) */
} tb_vrow;

typedef struct tb_textview {
    tb_widget base;
    tb_doc doc;
    tb_scroll sb[2];      /* [0] vertical (lines), [1] horizontal (pixels) */
    int grab_sb;          /* index of the bar being dragged, or -1         */
    int ar, ac;           /* selection anchor; equal to the caret = none   */
    int dragging;         /* selecting with the mouse                      */
    int modified;         /* edited since the last tb_textview_set()       */
    int wrap;             /* 1 = word wrap, no horizontal bar              */
    tb_vrow *vrows;       /* visual rows when wrap is on (see layout)      */
    int nvrows, cvrows;
    tb_action on_update;
    void *update_data;
} tb_textview;

/* Visible content rectangle: inside the padding, left of the vertical bar
 * and, unless wrapping, above the horizontal bar. */
static inline void tb_tv_content(const tb_textview *tv, int *cx, int *cy, int *cw, int *ch) {
    const tb_widget *w = &tv->base;
    *cx = w->x + TB_TEXT_PAD;
    *cy = w->y + TB_TEXT_PAD;
    *cw = w->w - TB_SCROLL_W - 2 * TB_TEXT_PAD;
    *ch = w->h - (tv->wrap ? 0 : TB_SCROLL_W) - 2 * TB_TEXT_PAD;
    if (*cw < 1) *cw = 1;
    if (*ch < 1) *ch = 1;
}


static inline int tb_tv_line_h(tb_app *t) {
    int lh = t->fascent + t->fdescent;
    return lh < 1 ? 1 : lh;
}

/* Tabs advance to the next multiple of TB_TAB_WIDTH columns. Columns are
 * counted from the start of the string passed in, so a wrapped row's tabs
 * are measured from that row's start. */
#ifndef TB_TAB_WIDTH
#define TB_TAB_WIDTH 4
#endif

/* Draw s[0..n) at x (when draw is non-zero) and return the x after it.
 * Runs of ordinary text are drawn with XDrawString; each '\t' moves to the
 * next tab stop. With draw == 0 this only measures. */
static inline int tb_tv_run(tb_app *t, int x, int base, int c, const char *s, int n, int draw) {
    int sw = XTextWidth(t->font, " ", 1);
    int col = 0, start = 0;
    for (int i = 0; i <= n; i++) {
        if (i < n && s[i] != '\t') continue;
        int len = i - start;
        if (len > 0) {
            if (draw) {
                tb_set_fg(t, c);
                XDrawString(t->dpy, t->buf, t->gc, x + t->ox, base + t->oy, s + start, len);
            }
            x += XTextWidth(t->font, s + start, len);
            col += len;
        }
        if (i < n) {
            int sp = TB_TAB_WIDTH - col % TB_TAB_WIDTH;
            x += sp * sw;
            col += sp;
            start = i + 1;
        }
    }
    return x;
}

/* Width in pixels of s[0..n), with tab stops. */
static inline int tb_tv_px(tb_app *t, const char *s, int n) {
    return tb_tv_run(t, 0, 0, 0, s, n, 0);
}

static inline void tb_tv_seg(tb_app *t, int x, int base, int c, const char *s, int n) {
    tb_tv_run(t, x, base, c, s, n, 1);
}

static inline int tb_tv_maxw(tb_app *t, const tb_doc *d) {
    int m = 0;
    for (int i = 0; i < d->n; i++) {
        int w = tb_tv_px(t, d->lines[i].s, d->lines[i].len);
        if (w > m) m = w;
    }
    return m;
}

/* Word wrap: split every logical line into visual rows that fit cw pixels.
 * A row breaks after the last space that fits, or inside a word if the word
 * is wider than the viewport. Rebuilt on every sync, so it tracks edits. */
static inline void tb_tv_layout(tb_app *t, tb_textview *tv, int cw) {
    tv->nvrows = 0;
    for (int r = 0; r < tv->doc.n; r++) {
        const tb_line *l = &tv->doc.lines[r];
        int start = 0;
        for (;;) {
            int end = start, brk = -1;
            while (end < l->len &&
                   tb_tv_px(t, l->s + start, end + 1 - start) <= cw) {
                if (l->s[end] == ' ') brk = end + 1;
                end++;
            }
            if (end >= l->len) {
                tb_vrow *v = tb_push((void **)&tv->vrows, &tv->nvrows, &tv->cvrows, sizeof *v);
                v->r = r; v->c0 = start; v->c1 = l->len;
                break;
            }
            int stop = brk > start ? brk : (end > start ? end : start + 1);
            tb_vrow *v = tb_push((void **)&tv->vrows, &tv->nvrows, &tv->cvrows, sizeof *v);
            v->r = r; v->c0 = start; v->c1 = stop;
            start = stop;
        }
    }
}

/* Index of the visual row holding the caret (wrap mode). A caret at a
 * wrap point belongs to the row that starts there. */
static inline int tb_tv_caret_vrow(const tb_textview *tv) {
    int best = 0;
    for (int i = 0; i < tv->nvrows; i++) {
        const tb_vrow *v = &tv->vrows[i];
        if (v->r == tv->doc.row && v->c0 <= tv->doc.col) best = i;
        else if (v->r > tv->doc.row) break;
    }
    return best;
}

/* Recompute scrollbar ranges from the document and viewport size. */
static inline void tb_tv_sync(tb_app *t, tb_textview *tv) {
    int cx, cy, cw, ch;
    tb_tv_content(tv, &cx, &cy, &cw, &ch);
    if (tv->wrap) {
        tb_tv_layout(t, tv, cw);
        tb_sb_set(&tv->sb[0], tv->nvrows, ch / tb_tv_line_h(t));
        tb_sb_set(&tv->sb[1], 1, 1);
        tv->sb[1].value = 0;
    } else {
        tb_sb_set(&tv->sb[0], tv->doc.n, ch / tb_tv_line_h(t));
        tb_sb_set(&tv->sb[1], tb_tv_maxw(t, &tv->doc), cw);
    }
    tv->sb[0].step = 1;
    tv->sb[1].step = TB_SCROLL_STEP;
}

/* Scroll so the caret is inside the viewport. */
static inline void tb_tv_ensure_cursor(tb_app *t, tb_textview *tv) {
    tb_tv_sync(t, tv);
    tb_scroll *v = &tv->sb[0], *h = &tv->sb[1];
    int row = tv->wrap ? tb_tv_caret_vrow(tv) : tv->doc.row;
    if (row < v->value) v->value = row;
    else if (row >= v->value + v->page) v->value = row - v->page + 1;
    tb_sb_clamp(v);
    if (tv->wrap) return;

    tb_line *l = &tv->doc.lines[row];
    int px = tb_tv_px(t, l->s, tv->doc.col);
    if (px < h->value) h->value = px;
    else if (px + TB_SCROLL_STEP > h->value + h->page)
        h->value = px + TB_SCROLL_STEP - h->page;
    tb_sb_clamp(h);
}

/* Put the caret at the character nearest to pointer position (x, y). */
static inline void tb_tv_click(tb_app *t, tb_textview *tv, int x, int y) {
    int cx, cy, cw, ch;
    tb_tv_content(tv, &cx, &cy, &cw, &ch);
    int row = tv->sb[0].value + (y - cy) / tb_tv_line_h(t);
    if (tv->wrap) {
        if (row < 0) row = 0;
        if (row >= tv->nvrows) row = tv->nvrows - 1;
        const tb_vrow *vr = &tv->vrows[row];
        const tb_line *l = &tv->doc.lines[vr->r];
        int xr = x - cx, best = vr->c0, bestd = -1;
        for (int c = vr->c0; c <= vr->c1; c++) {
            int d = tb_tv_px(t, l->s + vr->c0, c - vr->c0) - xr;
            if (d < 0) d = -d;
            if (bestd < 0 || d < bestd) { bestd = d; best = c; }
        }
        tv->doc.row = vr->r;
        tv->doc.col = best;
        return;
    }
    if (row < 0) row = 0;
    if (row >= tv->doc.n) row = tv->doc.n - 1;

    tb_line *l = &tv->doc.lines[row];
    int xr = x - cx + tv->sb[1].value;
    int best = 0, bestd = -1;
    for (int c = 0; c <= l->len; c++) {
        int d = tb_tv_px(t, l->s, c) - xr;
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


/* Word-wrap drawing: one visual row per line, no horizontal offset. */
static inline void tb_tv_draw_wrap(tb_app *t, tb_textview *tv, tb_widget *wb, int focused) {
    tb_tv_sync(t, tv);
    tb_fill(t, wb->x, wb->y, wb->w, wb->h, TB_C_BG);

    int cx, cy, cw, ch;
    tb_tv_content(tv, &cx, &cy, &cw, &ch);
    int lh = tb_tv_line_h(t);
    int top = tv->sb[0].value, page = tv->sb[0].page;
    int last = top + page;
    if (last >= tv->nvrows) last = tv->nvrows - 1;
    int sel = tb_tv_has_sel(tv);
    int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
    if (sel) tb_tv_sel_range(tv, &r1, &c1, &r2, &c2);

    tb_clip_rect(t, cx, cy, cw, ch);
    for (int i = top; i <= last; i++) {
        const tb_vrow *vr = &tv->vrows[i];
        const tb_line *l = &tv->doc.lines[vr->r];
        tb_tv_seg(t, cx, cy + (i - top) * lh + t->fascent, TB_C_FG,
                  l->s + vr->c0, vr->c1 - vr->c0);
    }

    /* selected parts of the visible rows: inverted background and text */
    if (sel) {
        for (int i = top; i <= last; i++) {
            const tb_vrow *vr = &tv->vrows[i];
            if (vr->r < r1 || vr->r > r2) continue;
            const tb_line *l = &tv->doc.lines[vr->r];
            int s0 = vr->r == r1 ? c1 : 0;
            int e0 = vr->r == r2 ? c2 : l->len;
            int a = s0 > vr->c0 ? s0 : vr->c0;
            int b = e0 < vr->c1 ? e0 : vr->c1;
            int brk = vr->r != r2 && vr->c1 == l->len;   /* line break selected */
            int x1 = cx + tb_tv_px(t, l->s + vr->c0, a - vr->c0);
            int x2 = cx + tb_tv_px(t, l->s + vr->c0, b - vr->c0);
            if (brk) x2 += t->fascent / 2 + 2;
            if (x2 <= x1) continue;
            int y0 = cy + (i - top) * lh;
            tb_clip_rect(t, x1, y0, x2 - x1, lh);
            tb_fill(t, x1, y0, x2 - x1, lh, TB_C_SEL_BG);
            tb_tv_seg(t, cx, y0 + t->fascent, TB_C_SEL_FG,
                      l->s + vr->c0, vr->c1 - vr->c0);
            tb_clip_rect(t, cx, cy, cw, ch);
        }
    }

    if (focused && !sel) {
        int i = tb_tv_caret_vrow(tv);
        if (i >= top && i <= top + page) {
            const tb_vrow *vr = &tv->vrows[i];
            const tb_line *l = &tv->doc.lines[vr->r];
            int px = cx + tb_tv_px(t, l->s + vr->c0, tv->doc.col - vr->c0);
            int y0 = cy + (i - top) * lh;
            tb_vline(t, px, y0, y0 + lh - 1, TB_C_FG);
        }
    }
    tb_clip_reset(t);

    tb_sb_draw(t, &tv->sb[0]);
    tb_frame(t, wb->x, wb->y, wb->w, wb->h, TB_C_FG);
}

static inline void tb_tv_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_textview *tv = (tb_textview *)wb;
    if (tv->wrap) {
        tb_tv_draw_wrap(t, tv, wb, focused);
        return;
    }
    tb_tv_sync(t, tv);
    tb_fill(t, wb->x, wb->y, wb->w, wb->h, TB_C_BG);

    int cx, cy, cw, ch;
    tb_tv_content(tv, &cx, &cy, &cw, &ch);
    int lh = tb_tv_line_h(t);
    int top = tv->sb[0].value, page = tv->sb[0].page;
    int hx = tv->sb[1].value;
    int sel = tb_tv_has_sel(tv);
    int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
    if (sel) tb_tv_sel_range(tv, &r1, &c1, &r2, &c2);

    tb_clip_rect(t, cx, cy, cw, ch);

    for (int r = top; r < tv->doc.n && r - top <= page; r++) {
        int base = cy + (r - top) * lh + t->fascent;
        tb_tv_seg(t, cx - hx, base, TB_C_FG, tv->doc.lines[r].s, (int)strlen(tv->doc.lines[r].s));
    }

    /* selected parts of the visible lines: inverted background and text */
    if (sel) {
        for (int r = top; r < tv->doc.n && r - top <= page; r++) {
            if (r < r1 || r > r2) continue;
            tb_line *l = &tv->doc.lines[r];
            int sc = r == r1 ? c1 : 0;
            int ec = r == r2 ? c2 : l->len;
            int x1 = cx - hx + tb_tv_px(t, l->s, sc);
            int x2 = cx - hx + tb_tv_px(t, l->s, ec);
            /* a selected line break shows as a small extra block */
            if (r != r2) x2 += t->fascent / 2 + 2;
            int y0 = cy + (r - top) * lh;
            if (x2 <= x1) continue;
            tb_clip_rect(t, x1, y0, x2 - x1, lh);
            tb_fill(t, x1, y0, x2 - x1, lh, TB_C_SEL_BG);
            tb_tv_seg(t, cx - hx, y0 + t->fascent, TB_C_SEL_FG, l->s, (int)strlen(l->s));
            tb_clip_rect(t, cx, cy, cw, ch);
        }
    }

    if (focused && !sel) {
        int r = tv->doc.row;
        if (r >= top && r - top <= page) {
            tb_line *l = &tv->doc.lines[r];
            int px = cx - hx + tb_tv_px(t, l->s, tv->doc.col);
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
    for (int k = 0; k < (tv->wrap ? 1 : 2); k++) {
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
    case XK_Tab:
        tb_tv_delete_sel(tv);
        tb_doc_insert_char(d, '\t');
        edited = 1;
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
        else if (c == '\t' || (c >= 32 && c < 127)) tb_doc_insert_char(&tv->doc, (char)c);
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

/* Vertical bar on the right, horizontal bar along the bottom (the latter
 * only when not wrapping). Follows the widget rectangle. */
static inline void tb_tv_place_bars(tb_textview *tv) {
    const tb_widget *b = &tv->base;
    tb_scroll *v = &tv->sb[0], *hz = &tv->sb[1];
    v->x = b->x + b->w - TB_SCROLL_W;
    v->y = b->y;
    v->w = TB_SCROLL_W;
    v->h = tv->wrap ? b->h : b->h - TB_SCROLL_W;
    hz->x = b->x;
    hz->y = b->y + b->h - TB_SCROLL_W;
    hz->w = b->w - TB_SCROLL_W;
    hz->h = TB_SCROLL_W;
}

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
    tv->base.takes_tab = 1;
    tv->grab_sb = -1;

    tb_tv_place_bars(tv);
    tv->sb[0].total = tv->sb[1].total = 1;
    tv->sb[0].page = tv->sb[1].page = 1;
    tv->sb[0].vertical = 1;
    tv->sb[1].vertical = 0;

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

/* Word wrap on or off. With wrap on, the horizontal bar is hidden and the
 * vertical bar runs the full height of the widget. */
static inline void tb_textview_set_wrap(tb_app *t, tb_textview *tv, int on) {
    tv->wrap = on;
    tb_tv_place_bars(tv);
    tv->sb[1].value = 0;
    tb_damage(t, &tv->base);
}

/* Move or resize the widget to (x, y, w, h); the bars follow. */
static inline void tb_textview_resize(tb_app *t, tb_textview *tv, int x, int y, int w, int h) {
    tv->base.x = x;
    tv->base.y = y;
    tv->base.w = w;
    tv->base.h = h;
    tb_tv_place_bars(tv);
    tb_damage_all(t);
}

/* Append text at the end and move the caret there, scrolling to show it.
 * For logs and transcripts. '\n' starts a new line; the flag is not touched. */
static inline void tb_textview_append(tb_app *t, tb_textview *tv, const char *s) {
    tb_doc *d = &tv->doc;
    d->row = d->n - 1;
    d->col = d->lines[d->row].len;
    for (; *s; s++) {
        if (*s == '\n') tb_doc_newline(d);
        else tb_doc_insert_char(d, *s);
    }
    tb_tv_collapse(tv);
    tb_tv_ensure_cursor(t, tv);
    tb_damage(t, &tv->base);
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
