#ifndef TB_LIST_H
#define TB_LIST_H

/*
 * tb_list.h - flat multi-column list (like a System 6 list box or the
 * Finder's "as list" view without the tree). Built on tb_view.h.
 */

#include "tb_view.h"

static inline tb_view *tb_list_new(tb_app *t, int x, int y, int w, int h,
                                   int ncols, const char **titles, const int *widths) {
    return tb_view_new(t, x, y, w, h, ncols, titles, widths, 0);
}

static inline tb_node *tb_list_add(tb_view *v, const char **cells, int ncells, void *data) {
    return tb_view_add(v, NULL, cells, ncells, data, 0);
}

#endif /* TB_LIST_H */
