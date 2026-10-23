#ifndef TB_MILLER_H
#define TB_MILLER_H

/*
 * tb_miller.h - Miller (Finder "column") view. Column 0 lists the root
 * nodes. Selecting a node in column k sets the selection for k and opens
 * column k+1 with its children, if the node is expandable. Selecting a
 * file (or a folder with no children) ends the chain. Selecting something
 * else in an earlier column closes the columns to its right.
 *
 * Nodes are tb_node values from tb_view.h, owned by the view; cells are
 * copied on add. Children are added with tb_miller_add(). Expandable
 * nodes are loaded lazily: on_expand runs the first time a node is
 * selected, and tb_miller_pending() returns that node inside it.
 *
 * Columns have equal width. When they do not all fit, the window shows
 * the columns around the focused one. Each column has its own scroll bar.
 *
 * Keys: Up/Down/Home/End/PgUp/PgDn move in the focused column,
 * Left/Right move the focus to the neighbouring column,
 * Return fires on_open. Click selects; double-click fires on_open.
 * The wheel scrolls the pane under the pointer and focuses it.
 */

#include "tb_view.h"

typedef struct tb_miller {
    tb_widget base;
    tb_node root;             /* pseudo node above column 0 (depth -1)     */
    tb_node **path;           /* path[k] = selected node in column k       */
    int npath, cpath;
    int focus;                /* column that receives keys and wheel       */
    int first;                /* leftmost visible column                   */
    int colw;                 /* preferred column width in pixels          */
    tb_scroll *sb;            /* one scroll bar per column                 */
    int csb;
    int drag_col;             /* column whose scroll thumb is dragged, -1  */
    tb_node *pending;         /* node being expanded inside on_expand      */
    tb_action on_select, on_open, on_expand;
    void *select_data, *open_data, *expand_data;
    unsigned long click_time;
    tb_node *click_node;
} tb_miller;

/* ---- geometry ----------------------------------------------------------- */

/* Number of columns on screen at once (at least one). */
static inline int tb_miller_nvis(const tb_miller *m) {
    int n = m->base.w / m->colw;
    return n < 1 ? 1 : n;
}

/* Number of columns in use: the root column, plus one for the selected
 * folder if there is one, and so on down the path. */
static inline int tb_miller_ncols(const tb_miller *m) {
    if (m->npath == 0) return 1;
    return m->npath + (m->path[m->npath - 1]->expandable ? 1 : 0);
}

/* The node whose children column k lists. */
static inline tb_node *tb_miller_parent(const tb_miller *m, int k) {
    return k == 0 ? (tb_node *)&m->root : m->path[k - 1];
}

/* Rectangle of visible column k (k must be in the visible range). The
 * last visible column absorbs any rounding remainder. */
static inline void tb_miller_colrect(const tb_miller *m, int k, int *x, int *w) {
    int nvis = tb_miller_nvis(m), cw = m->base.w / nvis;
    int slot = k - m->first;
    *x = m->base.x + slot * cw;
    *w = (slot == nvis - 1) ? m->base.w - slot * cw : cw;
}

/* Column under x, or -1. */
static inline int tb_miller_hit_col(const tb_miller *m, int x) {
    if (x < m->base.x || x >= m->base.x + m->base.w) return -1;
    int nvis = tb_miller_nvis(m), cw = m->base.w / nvis;
    int slot = (x - m->base.x) / cw;
    if (slot >= nvis) slot = nvis - 1;
    int k = m->first + slot;
    return k < tb_miller_ncols(m) ? k : -1;
}

static inline void tb_miller_reserve_sb(tb_miller *m, int n) {
    if (n <= m->csb) return;
    int cap = n + 8;
    tb_scroll *ns = realloc(m->sb, (size_t)cap * sizeof *ns);
    if (!ns) tb_oom();
    memset(ns + m->csb, 0, (size_t)(cap - m->csb) * sizeof *ns);
    m->sb = ns;
    m->csb = cap;
}

static inline void tb_miller_reserve_path(tb_miller *m, int n) {
    if (n <= m->cpath) return;
    int cap = n + 8;
    tb_node **np = realloc(m->path, (size_t)cap * sizeof *np);
    if (!np) tb_oom();
    m->path = np;
    m->cpath = cap;
}

/* Recompute column count, visible range and scroll bar geometry. Call
 * before drawing or hit-testing; it is cheap. */
static inline void tb_miller_layout(tb_app *t, tb_miller *m) {
    int ncols = tb_miller_ncols(m);
    int nvis = tb_miller_nvis(m);
    int rh = tb_view_rowh(t);
    tb_miller_reserve_sb(m, ncols);

    if (m->focus >= ncols) m->focus = ncols - 1;
    if (m->focus < 0) m->focus = 0;
    if (ncols <= nvis) {
        m->first = 0;
    } else {
        if (m->first > ncols - nvis) m->first = ncols - nvis;
        if (m->first < 0) m->first = 0;
        if (m->focus < m->first) m->first = m->focus;
        if (m->focus >= m->first + nvis) m->first = m->focus - nvis + 1;
    }

    int page = (m->base.h - 2) / rh;
    if (page < 1) page = 1;
    for (int k = 0; k < ncols; k++) {
        tb_scroll *s = &m->sb[k];
        int cx, cw;
        s->vertical = 1;
        s->step = 1;
        s->y = m->base.y + 1;
        s->h = m->base.h - 2;
        s->w = TB_SCROLL_W;
        if (k >= m->first && k < m->first + nvis) {
            tb_miller_colrect(m, k, &cx, &cw);
            s->x = cx + cw - 1 - TB_SCROLL_W;
        }
        tb_sb_set(s, tb_miller_parent(m, k)->nkids, page);
    }
}

/* Row index under (x, y) in column k, or -1. */
static inline int tb_miller_row_at(tb_app *t, const tb_miller *m, int k, int x, int y) {
    int cx, cw, rh = tb_view_rowh(t);
    tb_miller_colrect(m, k, &cx, &cw);
    int bx = cx + 1, bw = cw - 2 - TB_SCROLL_W;
    int by = m->base.y + 1, bh = m->base.h - 2;
    if (bw < 1 || x < bx || x >= bx + bw || y < by || y >= by + bh) return -1;
    int r = m->sb[k].value + (y - by) / rh;
    return r < tb_miller_parent(m, k)->nkids ? r : -1;
}

/* ---- model -------------------------------------------------------------- */

/* Add a node under parent (NULL = column 0). Returns the new node. Does not
 * repaint; call tb_damage() if the view is already on screen. */
static inline tb_node *tb_miller_add(tb_miller *m, tb_node *parent, const char **cells,
                                     int n, void *data, int expandable) {
    if (!parent) parent = &m->root;
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
    return nd;
}

/* Remove every node and reset the selection and scroll positions. */
static inline void tb_miller_clear(tb_miller *m) {
    for (int i = 0; i < m->root.nkids; i++) tb_node_free(m->root.kids[i]);
    free(m->root.kids);
    m->root.kids = NULL;
    m->root.nkids = m->root.ckids = 0;
    m->npath = 0;
    m->focus = 0;
    m->first = 0;
    m->pending = NULL;
    m->click_node = NULL;
    for (int k = 0; k < m->csb; k++) m->sb[k].value = 0;
}

static inline void tb_miller_on_select(tb_miller *m, tb_action cb, void *data) {
    m->on_select = cb;
    m->select_data = data;
}

static inline void tb_miller_on_open(tb_miller *m, tb_action cb, void *data) {
    m->on_open = cb;
    m->open_data = data;
}

static inline void tb_miller_on_expand(tb_miller *m, tb_action cb, void *data) {
    m->on_expand = cb;
    m->expand_data = data;
}

/* Node selected in the focused column, or NULL. */
static inline tb_node *tb_miller_selected(const tb_miller *m) {
    return m->focus < m->npath ? m->path[m->focus] : NULL;
}

/* Node being expanded; only valid inside the expand callback. */
static inline tb_node *tb_miller_pending(const tb_miller *m) {
    return m->pending;
}

/* Select child idx of column k. Closes the columns to the right of k,
 * loads the node's children if it is an unloaded folder, and reports the
 * selection. */
static inline void tb_miller_select(tb_app *t, tb_miller *m, int k, int idx) {
    tb_node *parent = tb_miller_parent(m, k);
    if (idx < 0 || idx >= parent->nkids) return;
    tb_node *n = parent->kids[idx];
    m->focus = k;
    if (k < m->npath && m->path[k] == n) return;   /* already selected */

    for (int j = k + 1; j < m->csb; j++) m->sb[j].value = 0;
    tb_miller_reserve_path(m, k + 1);
    m->path[k] = n;
    m->npath = k + 1;

    if (n->expandable && !n->loaded) {
        n->loaded = 1;
        if (m->on_expand) {
            m->pending = n;
            m->on_expand(t, m->expand_data);
            m->pending = NULL;
        }
    }

    /* Keep the selected row visible in its column. */
    tb_miller_layout(t, m);
    tb_scroll *s = &m->sb[k];
    if (idx < s->value) s->value = idx;
    else if (idx >= s->value + s->page) s->value = idx - s->page + 1;
    tb_sb_clamp(s);

    if (m->on_select) m->on_select(t, m->select_data);
}

/* ---- drawing ------------------------------------------------------------ */

static inline void tb_miller_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_miller *m = (tb_miller *)wb;
    (void)focused;
    tb_miller_layout(t, m);
    int rh = tb_view_rowh(t);
    int ncols = tb_miller_ncols(m);
    int nvis = tb_miller_nvis(m);

    tb_fill(t, wb->x, wb->y, wb->w, wb->h, TB_C_BG);
    for (int k = m->first; k < ncols && k < m->first + nvis; k++) {
        int cx, cw;
        tb_miller_colrect(m, k, &cx, &cw);
        tb_scroll *s = &m->sb[k];
        tb_node *parent = tb_miller_parent(m, k);
        int bx = cx + 1, bw = cw - 2 - TB_SCROLL_W;
        int by = wb->y + 1, bh = wb->h - 2;
        if (bw < 1) bw = 1;
        if (bh < 1) bh = 1;

        tb_clip_rect(t, bx, by, bw, bh);
        for (int r = s->value; r < parent->nkids && r - s->value < s->page; r++) {
            tb_node *n = parent->kids[r];
            int ry = by + (r - s->value) * rh;
            int sel = k < m->npath && m->path[k] == n;
            int active = sel && k == m->focus;
            if (active) tb_fill(t, bx, ry, bw, rh, TB_C_SEL_BG);
            else if (sel) tb_frame(t, bx, ry, bw, rh, TB_C_FG);
            int fg = active ? TB_C_SEL_FG : TB_C_FG;

            int maxw = bw - 2 * TB_CELL_PAD - (n->expandable ? 14 : 0);
            char buf[256];
            tb_view_fit(t, n->ncells > 0 ? n->cells[0] : "", maxw, buf, sizeof buf);
            tb_str(t, bx + TB_CELL_PAD, tb_text_base(t, ry, rh), fg, buf);
            if (n->expandable)
                tb_sb_arrow(t, bx + bw - 13, ry + (rh - 10) / 2, 10, 3, fg);
        }
        tb_clip_reset(t);

        tb_sb_draw(t, s);
        if (k > m->first)
            tb_vline(t, cx, wb->y + 1, wb->y + wb->h - 2, TB_C_FG);
    }
    tb_frame(t, wb->x, wb->y, wb->w, wb->h, TB_C_FG);
}

/* ---- input -------------------------------------------------------------- */

static inline int tb_miller_press(tb_app *t, tb_widget *wb, int x, int y) {
    tb_miller *m = (tb_miller *)wb;
    tb_miller_layout(t, m);
    int k = tb_miller_hit_col(m, x);
    if (k < 0) return 0;
    m->focus = k;

    tb_scroll *s = &m->sb[k];
    if (tb_sb_hit(s, x, y)) {
        tb_sb_press(s, x, y);
        m->drag_col = k;
        return 1;                       /* grab for thumb drags */
    }

    int idx = tb_miller_row_at(t, m, k, x, y);
    if (idx < 0) return 0;
    tb_node *n = tb_miller_parent(m, k)->kids[idx];
    tb_miller_select(t, m, k, idx);

    if (n == m->click_node && t->event_time - m->click_time < TB_DBLCLICK_MS) {
        m->click_node = NULL;
        if (m->on_open) m->on_open(t, m->open_data);
    } else {
        m->click_node = n;
        m->click_time = t->event_time;
    }
    return 0;
}

static inline void tb_miller_motion(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t;
    tb_miller *m = (tb_miller *)wb;
    if (m->drag_col >= 0 && m->drag_col < m->csb && m->sb[m->drag_col].dragging)
        tb_sb_drag(&m->sb[m->drag_col], x, y);
}

static inline void tb_miller_release(tb_app *t, tb_widget *wb, int x, int y) {
    (void)t; (void)x; (void)y;
    tb_miller *m = (tb_miller *)wb;
    if (m->drag_col >= 0 && m->drag_col < m->csb) m->sb[m->drag_col].dragging = 0;
    m->drag_col = -1;
}

/* The pane under the pointer scrolls and takes focus. */
static inline void tb_miller_wheel(tb_app *t, tb_widget *wb, int dir) {
    tb_miller *m = (tb_miller *)wb;
    tb_miller_layout(t, m);
    int k = tb_miller_hit_col(m, t->ptr_x);
    if (k < 0) return;
    m->focus = k;
    tb_sb_adjust(&m->sb[k], dir * 3);
}

static inline void tb_miller_key(tb_app *t, tb_widget *wb, XKeyEvent *e) {
    tb_miller *m = (tb_miller *)wb;
    KeySym ks = XLookupKeysym(e, 0);
    tb_miller_layout(t, m);
    int k = m->focus;
    tb_node *parent = tb_miller_parent(m, k);
    int cnt = parent->nkids;
    int idx = -1;
    if (k < m->npath)
        for (int i = 0; i < cnt; i++)
            if (parent->kids[i] == m->path[k]) { idx = i; break; }
    int page = m->sb[k].page;
    int ncols = tb_miller_ncols(m);
    int target = -1;

    switch (ks) {
    case XK_Up:
        target = idx <= 0 ? 0 : idx - 1;
        break;
    case XK_Down:
        target = idx < 0 ? 0 : (idx + 1 < cnt ? idx + 1 : idx);
        break;
    case XK_Home:
        target = 0;
        break;
    case XK_End:
        target = cnt - 1;
        break;
    case XK_Prior:
        target = idx - page < 0 ? 0 : idx - page;
        break;
    case XK_Next:
        target = idx + page >= cnt ? cnt - 1 : idx + page;
        break;
    case XK_Right:
    case XK_Left:
        /* Move focus between panes; the selections stay as they are. */
        if (ks == XK_Right ? k + 1 < ncols : k > 0) {
            m->focus = ks == XK_Right ? k + 1 : k - 1;
            if (m->on_select) m->on_select(t, m->select_data);
        }
        return;
    case XK_Return:
    case XK_KP_Enter:
        if (m->on_open && tb_miller_selected(m)) m->on_open(t, m->open_data);
        return;
    default:
        return;
    }

    if (target >= 0 && target < cnt) tb_miller_select(t, m, k, target);
}

static inline void tb_miller_destroy(tb_widget *wb) {
    tb_miller *m = (tb_miller *)wb;
    tb_miller_clear(m);
    free(m->root.kids);
    free(m->path);
    free(m->sb);
    free(m);
}

static const tb_wops TB_MILLER_OPS = {
    tb_miller_draw, tb_miller_press, tb_miller_motion, tb_miller_release,
    tb_miller_wheel, tb_miller_key, tb_miller_destroy, NULL
};

/* colw is the preferred column width; the view fits as many whole
 * columns as the width allows. */
static inline tb_miller *tb_miller_new(tb_app *t, int x, int y, int w, int h, int colw) {
    tb_miller *m = calloc(1, sizeof *m);
    if (!m) tb_oom();
    m->base.ops = &TB_MILLER_OPS;
    m->base.x = x;
    m->base.y = y;
    m->base.w = w;
    m->base.h = h;
    m->base.clickable = 1;
    m->base.focusable = 1;
    m->colw = colw > 0 ? colw : 160;
    m->root.depth = -1;
    m->drag_col = -1;
    tb_add_widget(t, &m->base);
    return m;
}

#endif /* TB_MILLER_H */
