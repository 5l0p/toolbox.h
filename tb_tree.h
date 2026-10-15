#ifndef TB_TREE_H
#define TB_TREE_H

/*
 * tb_tree.h - hierarchical multi-column tree (like a Finder list view
 * with disclosure triangles). The first column carries the tree.
 * Built on tb_view.h.
 *
 * Typical lazy use: create nodes with expandable=1 for folders, then
 * handle tb_view_on_expand() by adding the children of tb_view_pending().
 */

#include "tb_view.h"

static inline tb_view *tb_tree_new(tb_app *t, int x, int y, int w, int h,
                                   int ncols, const char **titles, const int *widths) {
    return tb_view_new(t, x, y, w, h, ncols, titles, widths, 1);
}

static inline tb_node *tb_tree_add(tb_view *v, tb_node *parent, const char **cells,
                                   int ncells, void *data, int expandable) {
    return tb_view_add(v, parent, cells, ncells, data, expandable);
}

#endif /* TB_TREE_H */
