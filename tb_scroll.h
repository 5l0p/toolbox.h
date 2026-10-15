#ifndef TB_SCROLL_H
#define TB_SCROLL_H

/*
 * tb_scroll.h - scrollbar state, geometry, input and drawing.
 * Not a widget by itself: tb_scrollbar.h wraps it as one, and
 * tb_textview.h embeds two of them. Units are whatever the caller uses
 * (lines, pixels, ...). Value range is 0 .. total - page.
 */

#include "tb_core.h"

typedef struct tb_scroll {
    int x, y, w, h;       /* on-screen rectangle            */
    int vertical;
    int total, page, value, step;
    int dragging, drag_off;
} tb_scroll;

static inline int tb_sb_len(const tb_scroll *s)   { return s->vertical ? s->h : s->w; }
static inline int tb_sb_thick(const tb_scroll *s) { return s->vertical ? s->w : s->h; }

static inline int tb_sb_max(const tb_scroll *s) {
    int m = s->total - s->page;
    return m > 0 ? m : 0;
}

static inline void tb_sb_clamp(tb_scroll *s) {
    int m = tb_sb_max(s);
    if (s->value < 0) s->value = 0;
    if (s->value > m) s->value = m;
}

static inline void tb_sb_set(tb_scroll *s, int total, int page) {
    s->total = total;
    s->page = page < 1 ? 1 : page;
    tb_sb_clamp(s);
}

/* Length of the track between the two arrow boxes. */
static inline int tb_sb_track(const tb_scroll *s) {
    int tr = tb_sb_len(s) - 2 * tb_sb_thick(s);
    return tr < 0 ? 0 : tr;
}

/* Thumb offset (from the bar's start, arrow box included) and length. */
static inline void tb_sb_thumb(const tb_scroll *s, int *off, int *len) {
    int thick = tb_sb_thick(s), track = tb_sb_track(s), m = tb_sb_max(s);
    int tl = track;
    if (m > 0) {
        tl = (int)((long)track * s->page / s->total);
        if (tl < TB_SCROLL_MIN_THUMB) tl = TB_SCROLL_MIN_THUMB;
        if (tl > track) tl = track;
    }
    int room = track - tl;
    *off = thick + ((m > 0 && room > 0) ? (int)((long)room * s->value / m) : 0);
    *len = tl;
}

static inline int tb_sb_hit(const tb_scroll *s, int x, int y) {
    return x >= s->x && x < s->x + s->w && y >= s->y && y < s->y + s->h;
}

enum { TB_SBP_DEC, TB_SBP_INC, TB_SBP_BEFORE, TB_SBP_AFTER, TB_SBP_THUMB };

static inline int tb_sb_part(const tb_scroll *s, int x, int y) {
    int p = s->vertical ? y - s->y : x - s->x;
    int len = tb_sb_len(s), thick = tb_sb_thick(s), off, tl;
    tb_sb_thumb(s, &off, &tl);
    if (p < thick) return TB_SBP_DEC;
    if (p >= len - thick) return TB_SBP_INC;
    if (p < off) return TB_SBP_BEFORE;
    if (p < off + tl) return TB_SBP_THUMB;
    return TB_SBP_AFTER;
}

/* Returns 1 if the value changed. */
static inline int tb_sb_adjust(tb_scroll *s, int delta) {
    int old = s->value;
    s->value += delta;
    tb_sb_clamp(s);
    return s->value != old;
}

/* Handle a button press on the bar. Returns 1 if the value changed.
 * Pressing the thumb starts a drag (s->dragging = 1). */
static inline int tb_sb_press(tb_scroll *s, int x, int y) {
    switch (tb_sb_part(s, x, y)) {
    case TB_SBP_DEC:    return tb_sb_adjust(s, -s->step);
    case TB_SBP_INC:    return tb_sb_adjust(s, s->step);
    case TB_SBP_BEFORE: return tb_sb_adjust(s, -s->page);
    case TB_SBP_AFTER:  return tb_sb_adjust(s, s->page);
    case TB_SBP_THUMB: {
        int off, tl;
        tb_sb_thumb(s, &off, &tl);
        s->dragging = 1;
        s->drag_off = (s->vertical ? y - s->y : x - s->x) - off;
        return 0;
    }
    }
    return 0;
}

/* Move the thumb to follow the pointer. Returns 1 if the value changed. */
static inline int tb_sb_drag(tb_scroll *s, int x, int y) {
    int off, tl, m = tb_sb_max(s);
    tb_sb_thumb(s, &off, &tl);
    int room = tb_sb_track(s) - tl;
    int p = (s->vertical ? y - s->y : x - s->x) - s->drag_off - tb_sb_thick(s);
    int old = s->value;
    s->value = (room > 0 && m > 0) ? (int)((long)p * m / room) : 0;
    tb_sb_clamp(s);
    return s->value != old;
}

static inline void tb_sb_arrow(tb_app *t, int bx, int by, int size, int dir, int c) {
    XPoint pts[3];
    int a = 4, b = size - 5, mid = size / 2;
    switch (dir) {
    case 0: /* up */
        pts[0].x = bx + mid; pts[0].y = by + a;
        pts[1].x = bx + a;   pts[1].y = by + b;
        pts[2].x = bx + b;   pts[2].y = by + b;
        break;
    case 1: /* down */
        pts[0].x = bx + mid; pts[0].y = by + b;
        pts[1].x = bx + a;   pts[1].y = by + a;
        pts[2].x = bx + b;   pts[2].y = by + a;
        break;
    case 2: /* left */
        pts[0].x = bx + a;   pts[0].y = by + mid;
        pts[1].x = bx + b;   pts[1].y = by + a;
        pts[2].x = bx + b;   pts[2].y = by + b;
        break;
    default: /* right */
        pts[0].x = bx + b;   pts[0].y = by + mid;
        pts[1].x = bx + a;   pts[1].y = by + a;
        pts[2].x = bx + a;   pts[2].y = by + b;
        break;
    }
    tb_poly(t, pts, 3, c);
}

static inline void tb_sb_draw(tb_app *t, const tb_scroll *s) {
    int thick = tb_sb_thick(s), off, tl;
    tb_fill(t, s->x, s->y, s->w, s->h, TB_C_BG);
    tb_frame(t, s->x, s->y, s->w, s->h, TB_C_FG);
    if (s->vertical) {
        tb_hline(t, s->x, s->x + s->w - 1, s->y + thick - 1, TB_C_FG);
        tb_hline(t, s->x, s->x + s->w - 1, s->y + s->h - thick, TB_C_FG);
        tb_sb_arrow(t, s->x, s->y, thick, 0, TB_C_FG);
        tb_sb_arrow(t, s->x, s->y + s->h - thick, thick, 1, TB_C_FG);
    } else {
        tb_vline(t, s->x + thick - 1, s->y, s->y + s->h - 1, TB_C_FG);
        tb_vline(t, s->x + s->w - thick, s->y, s->y + s->h - 1, TB_C_FG);
        tb_sb_arrow(t, s->x, s->y, thick, 2, TB_C_FG);
        tb_sb_arrow(t, s->x + s->w - thick, s->y, thick, 3, TB_C_FG);
    }
    tb_sb_thumb(s, &off, &tl);
    int tx = s->vertical ? s->x + 1 : s->x + off;
    int ty = s->vertical ? s->y + off : s->y + 1;
    int tw = s->vertical ? s->w - 2 : tl;
    int th = s->vertical ? tl : s->h - 2;
    tb_fill(t, tx, ty, tw, th, s->dragging ? TB_C_FG : TB_C_BG);
    tb_frame(t, tx, ty, tw, th, TB_C_FG);
}

#endif /* TB_SCROLL_H */
