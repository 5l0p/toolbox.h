#ifndef TB_VIEW_H
#define TB_VIEW_H

/*
 * tb_view.h - multi-column row view with a column header and a vertical
 * scrollbar. This is the engine behind tb_list.h (flat) and tb_tree.h
 * (hierarchical, with System 7 style disclosure triangles).
 *
 * Nodes are owned by the view. Node cells are copied on add.
 * Single selection. Click selects; double-click (or Return) fires the
 * "open" callback. In tree mode, clicking a triangle (or Left/Right keys)
 * expands/collapses; the "expand" callback fires the first time a node is
 * opened, so directories can be read lazily.
 */

#include "tb_core.h"
#include "tb_scroll.h"

typedef struct tb_node {
    char **cells;
    int ncells;
    int depth;            /* 0 = top level                              */
    int expandable;       /* show a triangle even before children exist */
    int expanded, loaded;
    void *data;           /* free for the caller to use                 */
    struct tb_node *parent;
    struct tb_node **kids;
    int nkids, ckids;
} tb_node;

typedef struct tb_view {
    tb_widget base;
    int tree;             /* 1 = hierarchical                          */
    int ncols;
    char **titles;
    int *widths;
    tb_node root;         /* pseudo node above the top level (depth -1) */
    tb_node **rows;       /* flattened visible nodes                    */
    int nrows, crows;
    tb_node *selected;
    tb_node *pending;     /* node being expanded inside on_expand       */
    tb_scroll sb;
    tb_action on_select, on_open, on_expand;
    void *select_data, *open_data, *expand_data;
    unsigned long click_time;
    tb_node *click_node;
} tb_view;

static inline int tb_view_rowh(tb_app *t) {
    return t->fascent + t->fdescent + 2 * TB_ROW_PAD;
}

/* Body rectangle (below the header, left of the scrollbar). */
static inline void tb_view_rects(tb_app *t, const tb_widget *w,
                                 int *bx, int *by, int *bw, int *bh) {
    int hh = tb_view_rowh(t);
    *bx = w->x + 1;
    *by = w->y + hh + 1;
    *bw = w->w - TB_SCROLL_W - 2;
    *bh = w->h - hh - 2;
    if (*bw < 1) *bw = 1;
    if (*bh < 1) *bh = 1;
}

/* Cut s down so that it fits in maxw pixels. */
static inline void tb_view_fit(tb_app *t, const char *s, int maxw, char *out, size_t cap) {
    snprintf(out, cap, "%s", s);
    size_t len = strlen(out);
    while (len > 0 && XTextWidth(t->font, out, (int)len) > maxw)
        out[--len] = 0;
}

/* ---- flattening and node management ----------------------------------- */

static inline void tb_view_collect(tb_view *v, const tb_node *p) {
    for (int i = 0; i < p->nkids; i++) {
        tb_node *k = p->kids[i];
        tb_node **slot = tb_push((void **)&v->rows, &v->nrows, &v->crows, sizeof *slot);
        *slot = k;
        if (v->tree && k->expanded) tb_view_collect(v, k);
    }
}

static inline void tb_view_flatten(tb_view *v) {
    v->nrows = 0;
    tb_view_collect(v, &v->root);
}

static inline void tb_node_free(tb_node *n) {
    for (int i = 0; i < n->ncells; i++) free(n->cells[i]);
    free(n->cells);
    for (int i = 0; i < n->nkids; i++) tb_node_free(n->kids[i]);
    free(n->kids);
    free(n);
}

static inline void tb_view_sync(tb_app *t, tb_view *v) {
    int bx, by, bw, bh;
    tb_view_rects(t, &v->base, &bx, &by, &bw, &bh);
    v->sb.x = v->base.x + v->base.w - 1 - TB_SCROLL_W;
    v->sb.y = by;
    v->sb.w = TB_SCROLL_W;
    v->sb.h = bh;
    v->sb.vertical = 1;
    v->sb.step = 1;
    tb_sb_set(&v->sb, v->nrows, bh / tb_view_rowh(t));
}

/* Add a node under parent (NULL = top level). Returns the new node. */
static inline tb_node *tb_view_add(tb_view *v, tb_node *parent, const char **cells,
                                   int n, void *data, int expandable) {
    if (!parent) parent = &v->root;
    tb_node *nd = calloc(1, sizeof *nd);
    if (!nd) tb_oom();
    nd->ncells = n;
    nd->cells = calloc(n > 0 ? (size_t)n : 1, sizeof *nd->cells);
    if (!nd->cells) tb_oom();
    for (int i = 0; i < n; i++) nd->cells[i] = tb_strdup(cells[i]);
    nd->depth = parent->depth + 1;
    nd->parent = parent;
    nd->expandable = expandable;
    nd->data = data;
    tb_node **slot = tb_push((void **)&parent->kids, &parent->nkids,
                             &parent->ckids, sizeof *slot);
    *slot = nd;
    tb_view_flatten(v);
    return nd;
}

static inline void tb_view_clear(tb_view *v) {
    for (int i = 0; i < v->root.nkids; i++) tb_node_free(v->root.kids[i]);
    free(v->root.kids);
    v->root.kids = NULL;
    v->root.nkids = v->root.ckids = 0;
    v->selected = NULL;
    v->pending = NULL;
    v->click_node = NULL;
    tb_view_flatten(v);
}

static inline tb_node *tb_view_selected(const tb_view *v) {
    return v->selected;
}

/* Node currently being expanded; only valid inside the expand callback. */
static inline tb_node *tb_view_pending(const tb_view *v) {
    return v->pending;
}

static inline void tb_view_on_select(tb_view *v, tb_action cb, void *data) {
    v->on_select = cb;
    v->select_data = data;
}

static inline void tb_view_on_open(tb_view *v, tb_action cb, void *data) {
    v->on_open = cb;
    v->open_data = data;
}

static inline void tb_view_on_expand(tb_view *v, tb_action cb, void *data) {
    v->on_expand = cb;
    v->expand_data = data;
}

static inline void tb_view_select(tb_app *t, tb_view *v, tb_node *n) {
    if (v->selected == n) return;
    v->selected = n;
    if (v->on_select) v->on_select(t, v->select_data);
}

static inline void tb_view_expand(tb_app *t, tb_view *v, tb_node *n, int open) {
    if (open) {
        if (n->expanded) return;
        n->expanded = 1;
        if (!n->loaded) {
            n->loaded = 1;
            if (v->on_expand) {
                v->pending = n;
                v->on_expand(t, v->expand_data);
                v->pending = NULL;
            }
        }
    } else {
        n->expanded = 0;
    }
    tb_view_flatten(v);
}

static inline int tb_view_index(const tb_view *v, const tb_node *n) {
    for (int i = 0; i < v->nrows; i++)
        if (v->rows[i] == n) return i;
    return -1;
}

static inline void tb_view_reveal(tb_view *v, int idx) {
    if (idx < v->sb.value) v->sb.value = idx;
    else if (idx >= v->sb.value + v->sb.page) v->sb.value = idx - v->sb.page + 1;
    tb_sb_clamp(&v->sb);
}

/* Does this node show a disclosure triangle? */
static inline int tb_view_has_tri(const tb_view *v, const tb_node *n) {
    return v->tree && (n->expandable || n->nkids > 0);
}

/* ---- drawing ----------------------------------------------------------- */

static inline void tb_view_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_view *v = (tb_view *)wb;
    (void)focused;
    tb_view_sync(t, v);
    int rh = tb_view_rowh(t);
    int bx, by, bw, bh;
    tb_view_rects(t, wb, &bx, &by, &bw, &bh);

    tb_fill(t, wb->x, wb->y, wb->w, wb->h, TB_C_BG);

    /* body rows, clipped to the body rectangle */
    XRectangle clip = { (short)bx, (short)by, (unsigned short)bw, (unsigned short)bh };
    XSetClipRectangles(t->dpy, t->gc, 0, 0, &clip, 1, Unsorted);
    int top = v->sb.value, page = v->sb.page;
    for (int r = top; r < v->nrows && r - top <= page; r++) {
        tb_node *n = v->rows[r];
        int ry = by + (r - top) * rh;
        int sel = (n == v->selected);
        if (sel) tb_fill(t, bx, ry, bw, rh, TB_C_SEL_BG);
        int fg = sel ? TB_C_SEL_FG : TB_C_FG;

        int x0 = bx;
        for (int c = 0; c < v->ncols; c++) {
            const char *s = c < n->ncells ? n->cells[c] : "";
            int tx = x0 + TB_CELL_PAD;
            if (c == 0 && v->tree) {
                int ax = x0 + TB_CELL_PAD + n->depth * TB_TREE_INDENT;
                if (tb_view_has_tri(v, n))
                    tb_sb_arrow(t, ax, ry + (rh - TB_TREE_DISC) / 2, TB_TREE_DISC,
                                n->expanded ? 1 : 3, fg);
                tx = ax + TB_TREE_DISC + TB_CELL_PAD;
            }
            char buf[256];
            tb_view_fit(t, s, x0 + v->widths[c] - TB_CELL_PAD - tx, buf, sizeof buf);
            tb_str(t, tx, tb_text_base(t, ry, rh), fg, buf);
            x0 += v->widths[c];
        }
    }
    XSetClipMask(t->dpy, t->gc, None);

    tb_sb_draw(t, &v->sb);
    tb_fill(t, v->sb.x, wb->y, TB_SCROLL_W, rh, TB_C_BG);

    /* column header */
    tb_hline(t, wb->x, wb->x + wb->w - 1, wb->y + rh, TB_C_FG);
    int cx = bx;
    for (int c = 0; c < v->ncols; c++) {
        char buf[256];
        tb_view_fit(t, v->titles[c], v->widths[c] - 2 * TB_CELL_PAD, buf, sizeof buf);
        tb_str(t, cx + TB_CELL_PAD, tb_text_base(t, wb->y, rh), TB_C_FG, buf);
        cx += v->widths[c];
        if (c + 1 < v->ncols && cx < bx + bw)
            tb_vline(t, cx, wb->y + 1, wb->y + rh - 1, TB_C_FG);
    }
    tb_frame(t, wb->x, wb->y, wb->w, wb->h, TB_C_FG);
}

/* ---- input ------------------------------------------------------------- */

/* Row under (x, y) in the body, or -1. Also returns the node. */
static inline int tb_view_row_at(tb_app *t, tb_view *v, int x, int y, tb_node **out) {
    int bx, by, bw, bh;
    tb_view_rects(t, &v->base, &bx, &by, &bw, &bh);
    if (x < bx || x >= bx + bw || y < by || y >= by + bh) return -1;
    int r = v->sb.value + (y - by) / tb_view_rowh(t);
    if (r < 0 || r >= v->nrows) return -1;
    *out = v->rows[r];
    return r;
}

static inline int tb_view_press(tb_app *t, tb_widget *wb, int x, int y) {
    tb_view *v = (tb_view *)wb;
    tb_view_sync(t, v);
    if (tb_sb_hit(&v->sb, x, y)) {
        tb_sb_press(&v->sb, x, y);
        return 1;                       /* grab for thumb drags */
    }

    tb_node *n = NULL;
    int r = tb_view_row_at(t, v, x, y, &n);
    if (r < 0) return 0;

    /* disclosure triangle? */
    if (tb_view_has_tri(v, n)) {
        int bx, by, bw, bh;
        tb_view_rects(t, wb, &bx, &by, &bw, &bh);
        int tx = bx + TB_CELL_PAD + n->depth * TB_TREE_INDENT;
        if (x >= tx && x < tx + TB_TREE_DISC) {
            tb_view_expand(t, v, n, !n->expanded);
            return 0;
        }
    }

    tb_view_select(t, v, n);
    if (n == v->click_node && t->event_time - v->click_time < TB_DBLCLICK_MS) {
        v->click_node = NULL;
        if (v->on_open) v->on_open(t, v->open_data);
    } else {
        v->click_node = n;
        v->click_time = t->event_time;
    }
    return 0;
}

static inline void tb_view_motion(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t;
    tb_view *v = (tb_view *)wb;
    if (v->sb.dragging) tb_sb_drag(&v->sb, x, y);
}

static inline void tb_view_release(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t; (void)x; (void)y;
    ((tb_view *)wb)->sb.dragging = 0;
}

static inline void tb_view_wheel(tb_app *t, tb_widget *wb, int dir) {
    (void)t;
    tb_view *v = (tb_view *)wb;
    tb_sb_adjust(&v->sb, dir * 3);
}

static inline void tb_view_key(tb_app *t, tb_widget *wb, XKeyEvent *e) {
    tb_view *v = (tb_view *)wb;
    KeySym ks = XLookupKeysym(e, 0);
    tb_view_sync(t, v);
    if (v->nrows == 0) return;
    int idx = v->selected ? tb_view_index(v, v->selected) : -1;
    int page = v->sb.page;
    int target = -1;

    switch (ks) {
    case XK_Up:
        target = idx <= 0 ? 0 : idx - 1;
        break;
    case XK_Down:
        target = idx < 0 ? 0 : (idx + 1 < v->nrows ? idx + 1 : idx);
        break;
    case XK_Home:
        target = 0;
        break;
    case XK_End:
        target = v->nrows - 1;
        break;
    case XK_Prior:
        target = idx - page < 0 ? 0 : idx - page;
        break;
    case XK_Next:
        target = idx + page >= v->nrows ? v->nrows - 1 : idx + page;
        break;
    case XK_Right:
        if (v->tree && v->selected && tb_view_has_tri(v, v->selected)) {
            if (!v->selected->expanded) {
                tb_view_expand(t, v, v->selected, 1);
            } else if (idx + 1 < v->nrows && v->rows[idx + 1]->parent == v->selected) {
                target = idx + 1;
            }
        }
        break;
    case XK_Left:
        if (v->tree && v->selected) {
            if (v->selected->expanded) {
                tb_view_expand(t, v, v->selected, 0);
            } else if (v->selected->parent && v->selected->parent->depth >= 0) {
                target = tb_view_index(v, v->selected->parent);
            }
        }
        break;
    case XK_Return:
    case XK_KP_Enter:
        if (v->on_open && v->selected) v->on_open(t, v->open_data);
        return;
    default:
        return;
    }

    if (target >= 0 && target < v->nrows) {
        tb_view_select(t, v, v->rows[target]);
        tb_view_reveal(v, target);
    }
}

static inline void tb_view_destroy(tb_widget *wb) {
    tb_view *v = (tb_view *)wb;
    tb_view_clear(v);
    for (int i = 0; i < v->ncols; i++) free(v->titles[i]);
    free(v->titles);
    free(v->widths);
    free(v->rows);
    free(v->root.kids);
    free(v);
}

static const tb_wops TB_VIEW_OPS = {
    tb_view_draw, tb_view_press, tb_view_motion, tb_view_release,
    tb_view_wheel, tb_view_key, tb_view_destroy
};

/* Use tb_list_new() or tb_tree_new() rather than calling this directly. */
static inline tb_view *tb_view_new(tb_app *t, int x, int y, int w, int h,
                                   int ncols, const char **titles, const int *widths,
                                   int tree) {
    tb_view *v = calloc(1, sizeof *v);
    if (!v) tb_oom();
    v->base.ops = &TB_VIEW_OPS;
    v->base.x = x;
    v->base.y = y;
    v->base.w = w;
    v->base.h = h;
    v->base.clickable = 1;
    v->base.focusable = 1;
    v->tree = tree ? 1 : 0;
    v->ncols = ncols;
    v->titles = calloc((size_t)ncols, sizeof *v->titles);
    v->widths = calloc((size_t)ncols, sizeof *v->widths);
    if (!v->titles || !v->widths) tb_oom();
    for (int i = 0; i < ncols; i++) {
        v->titles[i] = tb_strdup(titles[i]);
        v->widths[i] = widths[i];
    }
    v->root.depth = -1;
    tb_add_widget(t, &v->base);
    return v;
}

#endif /* TB_VIEW_H */
