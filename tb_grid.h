#ifndef TB_GRID_H
#define TB_GRID_H

/*
 * tb_grid.h - spreadsheet-style grid of text cells, with column letters and
 * row numbers, scroll bars, one selected cell and in-place editing.
 *
 *   click, arrows, Home/End, PgUp/PgDn, wheel   move the selection
 *   double-click or F2                          edit the selected cell
 *   typing                                      start editing, replacing it
 *   Return / Tab / arrows                       commit, then move
 *                                               (Return down, Tab right)
 *   Escape                                      cancel the edit
 *   drag a column edge in the header            resize that column
 *
 * The grid stores each cell's raw text. The optional show callback gives the
 * text drawn in a cell, so a spreadsheet can show a formula's value. The
 * string it returns must stay valid until the grid is drawn again.
 */

#include "tb_core.h"
#include "tb_scroll.h"
#include "tb_view.h"

#define TB_GRID_HDR_W 36       /* width of the row-number column */
#define TB_GRID_MIN_COLW 20    /* narrowest a column can be resized to */
#define TB_GRID_EDGE 3         /* pixels either side of a column edge that grab it */

enum { TB_GRID_GRAB_ROWS = 0, TB_GRID_GRAB_COLS = 1, TB_GRID_GRAB_RESIZE = 2 };

typedef struct tb_grid tb_grid;
typedef const char *(*tb_grid_show_fn)(tb_grid *g, int r, int c, void *data);

struct tb_grid {
    tb_widget base;
    int rows, cols;
    int rh;                   /* row height                             */
    int *cwid;                /* width of each column in pixels         */
    char **text;              /* rows * cols raw strings, NULL = empty  */
    int sr, sc;               /* selected cell                          */
    tb_scroll sb[2];          /* [0] vertical (rows), [1] horizontal (cols) */
    int grab;                 /* GRAB_* below, or -1 = none             */
    int rcol, rx0, rw0;       /* column being resized, pointer x, its width */
    int editing;
    char *edit;               /* text of the cell being edited          */
    int elen, ecap;
    unsigned long click_time;
    int click_r, click_c;
    tb_action on_select, on_change, on_edit;
    void *select_data, *change_data, *edit_data;
    tb_grid_show_fn show;
    void *show_data;
};

/* Body rectangle: the cells, left of and below the headers and scroll bars. */
static inline void tb_grid_body(const tb_grid *g, int *bx, int *by, int *bw, int *bh) {
    *bx = g->base.x + TB_GRID_HDR_W;
    *by = g->base.y + g->rh;
    *bw = g->base.w - TB_GRID_HDR_W - TB_SCROLL_W;
    *bh = g->base.h - g->rh - TB_SCROLL_W;
    if (*bw < 1) *bw = 1;
    if (*bh < 1) *bh = 1;
}

/* Pixel width of a column. */
static inline int tb_grid_colw(const tb_grid *g, int c) {
    return g->cwid[c];
}

/* How many columns, starting at left, fit in bw pixels (at least one). */
static inline int tb_grid_fit(const tb_grid *g, int left, int bw) {
    int n = 0, used = 0;
    for (int c = left; c < g->cols; c++) {
        used += g->cwid[c];
        if (used > bw && n > 0) break;
        n++;
    }
    return n < 1 ? 1 : n;
}

/* Set a column's width. Clamped to TB_GRID_MIN_COLW. */
static inline void tb_grid_set_colw(tb_grid *g, int c, int w) {
    if (c < 0 || c >= g->cols) return;
    g->cwid[c] = w < TB_GRID_MIN_COLW ? TB_GRID_MIN_COLW : w;
    if (g->base.app) tb_damage(g->base.app, &g->base);
}

/* Place the scroll bars and set their ranges. Call before drawing or input. */
static inline void tb_grid_sync(tb_grid *g) {
    int bx, by, bw, bh;
    tb_grid_body(g, &bx, &by, &bw, &bh);
    int prow = bh / g->rh;
    int pcol = tb_grid_fit(g, g->sb[1].value < g->cols ? g->sb[1].value : 0, bw);
    if (prow < 1) prow = 1;
    if (pcol < 1) pcol = 1;
    tb_scroll *v = &g->sb[0], *h = &g->sb[1];
    v->vertical = 1;
    v->step = 1;
    v->x = g->base.x + g->base.w - TB_SCROLL_W;
    v->y = by;
    v->w = TB_SCROLL_W;
    v->h = bh;
    h->vertical = 0;
    h->step = 1;
    h->x = bx;
    h->y = g->base.y + g->base.h - TB_SCROLL_W;
    h->w = bw;
    h->h = TB_SCROLL_W;
    tb_sb_set(v, g->rows, prow);
    tb_sb_set(h, g->cols, pcol);
}

/* Scroll so the selected cell is visible. */
static inline void tb_grid_reveal(tb_grid *g) {
    tb_grid_sync(g);
    tb_scroll *v = &g->sb[0], *h = &g->sb[1];
    if (g->sr < v->value) v->value = g->sr;
    else if (g->sr >= v->value + v->page) v->value = g->sr - v->page + 1;
    tb_sb_clamp(v);
    if (g->sc < h->value) {
        h->value = g->sc;
    } else {
        /* scroll right until the selected column fits in the body */
        int bx, by, bw, bh;
        tb_grid_body(g, &bx, &by, &bw, &bh);
        int used = 0;
        for (int c = h->value; c <= g->sc; c++) used += g->cwid[c];
        while (h->value < g->sc && used > bw) {
            used -= g->cwid[h->value];
            h->value++;
        }
    }
    tb_sb_clamp(h);
}

/* Raw text of a cell ("" if empty). Valid until the cell changes. */
static inline const char *tb_grid_get(const tb_grid *g, int r, int c) {
    const char *s = g->text[r * g->cols + c];
    return s ? s : "";
}

/* Replace a cell's raw text. An empty string makes the cell empty. */
static inline void tb_grid_set(tb_grid *g, int r, int c, const char *s) {
    char **slot = &g->text[r * g->cols + c];
    free(*slot);
    *slot = (s && *s) ? tb_strdup(s) : NULL;
    if (g->base.app) tb_damage(g->base.app, &g->base);
}

/* Empty every cell. Does not call on_change. */
static inline void tb_grid_clear(tb_grid *g) {
    for (int i = 0; i < g->rows * g->cols; i++) {
        free(g->text[i]);
        g->text[i] = NULL;
    }
    if (g->base.app) tb_damage(g->base.app, &g->base);
}

static inline void tb_grid_on_select(tb_grid *g, tb_action cb, void *data) {
    g->on_select = cb;
    g->select_data = data;
}

static inline void tb_grid_on_change(tb_grid *g, tb_action cb, void *data) {
    g->on_change = cb;
    g->change_data = data;
}

/* Called whenever the text being typed into a cell changes (and when an
 * edit starts or is cancelled), so a formula bar can mirror it. */
static inline void tb_grid_on_edit(tb_grid *g, tb_action cb, void *data) {
    g->on_edit = cb;
    g->edit_data = data;
}

/* Repaint and tell the app the edit text changed. */
static inline void tb_grid_edited(tb_app *t, tb_grid *g) {
    tb_damage(t, &g->base);
    if (g->on_edit) g->on_edit(t, g->edit_data);
}

static inline void tb_grid_on_show(tb_grid *g, tb_grid_show_fn fn, void *data) {
    g->show = fn;
    g->show_data = data;
}

/* Select a cell (clamped to the grid). Does not start editing. */
static inline void tb_grid_select(tb_app *t, tb_grid *g, int r, int c) {
    if (r < 0) r = 0;
    if (c < 0) c = 0;
    if (r >= g->rows) r = g->rows - 1;
    if (c >= g->cols) c = g->cols - 1;
    if (r == g->sr && c == g->sc) return;
    g->sr = r;
    g->sc = c;
    tb_grid_reveal(g);
    if (g->on_select) g->on_select(t, g->select_data);
    tb_damage(t, &g->base);
}

static inline void tb_grid_begin(tb_grid *g, const char *initial) {
    size_t n = strlen(initial);
    if ((int)n + 2 > g->ecap) {
        g->ecap = (int)n + 16;
        g->edit = realloc(g->edit, (size_t)g->ecap);
        if (!g->edit) tb_oom();
    }
    memcpy(g->edit, initial, n + 1);
    g->elen = (int)n;
    g->editing = 1;
}

static inline void tb_grid_append(tb_grid *g, char ch) {
    if (g->elen + 2 > g->ecap) {
        g->ecap = g->elen + 16;
        g->edit = realloc(g->edit, (size_t)g->ecap);
        if (!g->edit) tb_oom();
    }
    g->edit[g->elen++] = ch;
    g->edit[g->elen] = 0;
}

/* Store the edited text in the selected cell and fire on_change. */
static inline void tb_grid_commit(tb_app *t, tb_grid *g) {
    if (!g->editing) return;
    g->editing = 0;
    tb_grid_set(g, g->sr, g->sc, g->edit ? g->edit : "");
    if (g->on_change) g->on_change(t, g->change_data);
}

/* Text drawn in a cell: the show callback's text, or the raw text. */
static inline const char *tb_grid_shown(tb_grid *g, int r, int c) {
    if (g->show) return g->show(g, r, c, g->show_data);
    return tb_grid_get(g, r, c);
}

/* ---- drawing ------------------------------------------------------------ */

static inline void tb_grid_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_grid *g = (tb_grid *)wb;
    (void)focused;
    tb_grid_sync(g);
    int bx, by, bw, bh;
    tb_grid_body(g, &bx, &by, &bw, &bh);
    int rh = g->rh;
    int top = g->sb[0].value, left = g->sb[1].value;
    int prow = g->sb[0].page;

    tb_fill(t, wb->x, wb->y, wb->w, wb->h, TB_C_BG);

    /* column letters, with a rule at the right edge of each column */
    tb_clip_rect(t, bx, wb->y, bw, rh);
    int hx = bx;
    for (int c = left; c < g->cols && hx < bx + bw; c++) {
        char lab[2] = { (char)('A' + c), 0 };
        tb_str(t, hx + TB_CELL_PAD, tb_text_base(t, wb->y, rh), TB_C_FG, lab);
        hx += g->cwid[c];
        tb_vline(t, hx - 1, wb->y, by - 2, TB_C_FG);
    }
    tb_clip_reset(t);

    /* row numbers */
    tb_clip_rect(t, wb->x, by, TB_GRID_HDR_W, bh);
    for (int r = top; r < g->rows && r < top + prow; r++) {
        char num[16];
        snprintf(num, sizeof num, "%d", r + 1);
        tb_str(t, wb->x + 4, tb_text_base(t, by + (r - top) * rh, rh), TB_C_FG, num);
    }
    tb_clip_reset(t);

    /* cells */
    tb_clip_rect(t, bx, by, bw, bh);
    for (int r = top; r < g->rows && r < top + prow; r++) {
        int x = bx;
        for (int c = left; c < g->cols && x < bx + bw; c++) {
            int cw = g->cwid[c];
            int y = by + (r - top) * rh;
            int sel = (r == g->sr && c == g->sc);
            char buf[256];
            const char *s = sel && g->editing ? g->edit : tb_grid_shown(g, r, c);
            if (sel && !g->editing) tb_fill(t, x + 1, y + 1, cw - 1, rh - 1, TB_C_SEL_BG);
            int fg = sel && !g->editing ? TB_C_SEL_FG : TB_C_FG;
            tb_view_fit(t, s, cw - 2 * TB_CELL_PAD, buf, sizeof buf);
            tb_str(t, x + TB_CELL_PAD, tb_text_base(t, y, rh), fg, buf);
            if (sel && g->editing) {
                int px = x + TB_CELL_PAD + tb_text_w(t, buf);
                if (px < x + cw - 2) tb_vline(t, px, y + 2, y + rh - 3, TB_C_FG);
            }
            tb_hline(t, x, x + cw - 1, y + rh - 1, TB_C_FG);
            tb_vline(t, x + cw - 1, y, y + rh - 1, TB_C_FG);
            x += cw;
        }
    }
    tb_clip_reset(t);

    /* header separators */
    tb_hline(t, wb->x, wb->x + wb->w - 1, by - 1, TB_C_FG);
    tb_vline(t, bx - 1, wb->y, wb->y + wb->h - 1, TB_C_FG);

    tb_sb_draw(t, &g->sb[0]);
    tb_sb_draw(t, &g->sb[1]);
    tb_fill(t, g->sb[0].x, g->sb[1].y, TB_SCROLL_W, TB_SCROLL_W, TB_C_BG);
    tb_frame(t, wb->x, wb->y, wb->w, wb->h, TB_C_FG);
}

/* ---- input -------------------------------------------------------------- */

/* Column whose header right edge is within TB_GRID_EDGE of x, or -1. */
static inline int tb_grid_edge_at(const tb_grid *g, int x) {
    int bx, by, bw, bh;
    tb_grid_body(g, &bx, &by, &bw, &bh);
    int ex = bx;
    for (int c = g->sb[1].value; c < g->cols && ex <= bx + bw; c++) {
        ex += g->cwid[c];
        if (x >= ex - TB_GRID_EDGE && x <= ex + TB_GRID_EDGE) return c;
    }
    return -1;
}

/* Column under x in the body, or -1. */
static inline int tb_grid_col_at(const tb_grid *g, int x) {
    int bx, by, bw, bh;
    tb_grid_body(g, &bx, &by, &bw, &bh);
    int cx = bx;
    for (int c = g->sb[1].value; c < g->cols && cx < bx + bw; c++) {
        cx += g->cwid[c];
        if (x < cx) return c;
    }
    return -1;
}

/* A press on a scroll bar starts a drag. A press on a column edge in the
 * header starts resizing that column. A press on a cell selects it, and a
 * second press on the same cell within the double-click time edits it. */
static inline int tb_grid_press(tb_app *t, tb_widget *wb, int x, int y) {
    tb_grid *g = (tb_grid *)wb;
    tb_grid_sync(g);
    if (tb_sb_hit(&g->sb[0], x, y)) {
        tb_sb_press(&g->sb[0], x, y);
        g->grab = TB_GRID_GRAB_ROWS;
        return 1;
    }
    if (tb_sb_hit(&g->sb[1], x, y)) {
        tb_sb_press(&g->sb[1], x, y);
        g->grab = TB_GRID_GRAB_COLS;
        return 1;
    }
    int bx, by, bw, bh;
    tb_grid_body(g, &bx, &by, &bw, &bh);
    if (y >= g->base.y && y < by - 1 && x >= bx) {
        int c = tb_grid_edge_at(g, x);
        if (c >= 0) {
            g->grab = TB_GRID_GRAB_RESIZE;
            g->rcol = c;
            g->rx0 = x;
            g->rw0 = g->cwid[c];
            return 1;
        }
        return 0;
    }
    if (x < bx || x >= bx + bw || y < by || y >= by + bh) return 0;
    int r = g->sb[0].value + (y - by) / g->rh;
    int c = tb_grid_col_at(g, x);
    if (r >= g->rows || c < 0) return 0;

    tb_grid_commit(t, g);
    tb_grid_select(t, g, r, c);
    if (r == g->click_r && c == g->click_c && t->event_time - g->click_time < TB_DBLCLICK_MS) {
        tb_grid_begin(g, tb_grid_get(g, r, c));
        g->click_r = g->click_c = -1;
        tb_grid_edited(t, g);
    } else {
        g->click_r = r;
        g->click_c = c;
        g->click_time = t->event_time;
    }
    return 0;
}

static inline void tb_grid_motion(tb_app *t, tb_widget *wb, int x, int y) {
    tb_grid *g = (tb_grid *)wb;
    if (g->grab == TB_GRID_GRAB_RESIZE) {
        int w = g->rw0 + (x - g->rx0);
        if (w < TB_GRID_MIN_COLW) w = TB_GRID_MIN_COLW;
        if (w != g->cwid[g->rcol]) {
            g->cwid[g->rcol] = w;
            tb_damage(t, &g->base);
        }
    } else if (g->grab >= 0) {
        tb_sb_drag(&g->sb[g->grab], x, y);
    }
}

static inline void tb_grid_release(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t; (void)x; (void)y;
    tb_grid *g = (tb_grid *)wb;
    if (g->grab == TB_GRID_GRAB_ROWS || g->grab == TB_GRID_GRAB_COLS)
        g->sb[g->grab].dragging = 0;
    g->grab = -1;
}

static inline void tb_grid_wheel(tb_app *t, tb_widget *wb, int dir) {
    tb_grid *g = (tb_grid *)wb;
    tb_grid_sync(g);
    tb_sb_adjust(&g->sb[0], dir * 3);
    tb_damage(t, &g->base);
}

static inline void tb_grid_key(tb_app *t, tb_widget *wb, XKeyEvent *e) {
    tb_grid *g = (tb_grid *)wb;
    KeySym ks;
    char buf[8];
    int n = XLookupString(e, buf, sizeof buf, &ks, NULL);
    int page = g->sb[0].page > 0 ? g->sb[0].page : 1;
    int dr = 0, dc = 0;

    if (g->editing) {
        switch (ks) {
        case XK_Escape:
            g->editing = 0;
            tb_grid_edited(t, g);
            return;
        case XK_BackSpace:
            if (g->elen > 0) g->edit[--g->elen] = 0;
            tb_grid_edited(t, g);
            return;
        case XK_Return:
        case XK_KP_Enter:
            dr = 1;
            break;
        case XK_Tab:
            dc = 1;
            break;
        case XK_Up:    dr = -1; break;
        case XK_Down:  dr = 1;  break;
        case XK_Left:  dc = -1; break;
        case XK_Right: dc = 1;  break;
        default:
            if (n == 1 && buf[0] >= 32 && buf[0] < 127) {
                tb_grid_append(g, buf[0]);
                tb_grid_edited(t, g);
            }
            return;
        }
        tb_grid_commit(t, g);
        tb_grid_select(t, g, g->sr + dr, g->sc + dc);
        tb_damage(t, &g->base);
        return;
    }

    switch (ks) {
    case XK_Up:       dr = -1; break;
    case XK_Down:     dr = 1;  break;
    case XK_Left:     dc = -1; break;
    case XK_Right:    dc = 1;  break;
    case XK_Prior:    dr = -page; break;
    case XK_Next:     dr = page;  break;
    case XK_Home:     tb_grid_select(t, g, g->sr, 0); return;
    case XK_End:      tb_grid_select(t, g, g->sr, g->cols - 1); return;
    case XK_Return:
    case XK_KP_Enter: dr = 1; break;
    case XK_Tab:      dc = 1; break;
    case XK_F2:
        tb_grid_begin(g, tb_grid_get(g, g->sr, g->sc));
        tb_grid_edited(t, g);
        return;
    default:
        if (n == 1 && buf[0] >= 32 && buf[0] < 127) {
            tb_grid_begin(g, "");
            tb_grid_append(g, buf[0]);
            tb_grid_edited(t, g);
        }
        return;
    }
    tb_grid_select(t, g, g->sr + dr, g->sc + dc);
}

static inline void tb_grid_destroy(tb_widget *wb) {
    tb_grid *g = (tb_grid *)wb;
    for (int i = 0; i < g->rows * g->cols; i++) free(g->text[i]);
    free(g->text);
    free(g->cwid);
    free(g->edit);
    free(g);
}

static const tb_wops TB_GRID_OPS = {
    tb_grid_draw, tb_grid_press, tb_grid_motion, tb_grid_release,
    tb_grid_wheel, tb_grid_key, tb_grid_destroy, NULL
};

/* A grid of rows x cols empty cells, colw pixels wide to start with. Rows and
 * columns are fixed for the life of the grid; column widths can be changed
 * by dragging the header edges or with tb_grid_set_colw(). */
static inline tb_grid *tb_grid_new(tb_app *t, int x, int y, int w, int h,
                                   int rows, int cols, int colw) {
    tb_grid *g = calloc(1, sizeof *g);
    if (!g) tb_oom();
    g->base.ops = &TB_GRID_OPS;
    g->base.x = x;
    g->base.y = y;
    g->base.w = w;
    g->base.h = h;
    g->base.clickable = 1;
    g->base.focusable = 1;
    g->base.takes_tab = 1;
    g->rows = rows;
    g->cols = cols;
    if (colw < TB_GRID_MIN_COLW) colw = 80;
    g->cwid = malloc((size_t)cols * sizeof *g->cwid);
    if (!g->cwid) tb_oom();
    for (int c = 0; c < cols; c++) g->cwid[c] = colw;
    g->rh = tb_view_rowh(t);
    g->text = calloc((size_t)rows * (size_t)cols, sizeof *g->text);
    if (!g->text) tb_oom();
    g->grab = -1;
    g->click_r = g->click_c = -1;
    tb_add_widget(t, &g->base);
    return g;
}

#endif /* TB_GRID_H */
