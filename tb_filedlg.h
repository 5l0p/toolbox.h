#ifndef TB_FILEDLG_H
#define TB_FILEDLG_H

/*
 * tb_filedlg.h - standard Open and Save As dialogs, built on the modal
 * dialog, the list view and the field.
 *
 *   char path[TB_PATH_MAX];
 *   if (tb_open_dialog(app, ".", path, sizeof path)) { ... }
 *   if (tb_save_dialog(app, ".", "notes.txt", path, sizeof path)) { ... }
 *
 * Both return 1 and fill `out` with the chosen path, or return 0 on Cancel.
 *
 * In the list: click selects, double-click (or Return) opens a folder or
 * chooses a file. Up goes to the parent folder. In Save As, the name field
 * follows the selection and Return saves.
 */

#include "tb_fs.h"
#include "tb_list.h"
#include "tb_field.h"
#include "tb_label.h"
#include "tb_button.h"
#include "tb_dialog.h"
#include "tb_view.h"

#define TB_FD_W 380
#define TB_FD_H 300

typedef struct tb_filedlg {
    tb_dialog *d;
    tb_view *view;
    tb_label *pathlbl;
    tb_field *name;          /* Save As only */
    int save;
    char dir[TB_PATH_MAX];
    char result[2 * TB_PATH_MAX];   /* room for dir + "/" + name */
    int pathw;               /* room for the path label */
} tb_filedlg;

/* Show the folder path. Long paths keep their end (the current folder). */
static inline void tb_fd_show_path(tb_app *t, tb_filedlg *fd) {
    size_t len = strlen(fd->dir), k = 0;
    while (k < len && tb_text_w(t, fd->dir + k) > fd->pathw) k++;
    tb_label_set(t, fd->pathlbl, fd->dir + k);
}

/* List the contents of dir in the view. */
static inline void tb_fd_load(tb_app *t, tb_filedlg *fd, const char *dir) {
    snprintf(fd->dir, sizeof fd->dir, "%s", dir);
    tb_view_clear(fd->view);
    int n = 0;
    tb_fs_entry *e = tb_fs_list(fd->dir, &n);
    for (int i = 0; i < n; i++) {
        char size[32], kind[64];
        if (e[i].isdir) size[0] = 0;
        else tb_fs_format_size(e[i].size, size, sizeof size);
        tb_fs_kind(e[i].name, e[i].isdir, kind, sizeof kind);
        const char *cells[3] = { e[i].name, size, kind };
        tb_node *nd = tb_view_add(fd->view, NULL, cells, 3, NULL, e[i].isdir);
        tb_view_set_key(nd, 1, (double)e[i].size);
    }
    tb_fs_free(e, n);
    /* keys were set after insertion: sort again with them */
    tb_view_sort(fd->view, fd->view->sort_col, fd->view->sort_desc);
    tb_fd_show_path(t, fd);
}

/* Accept the current choice: a file from the list, or the name field. */
static inline void tb_fd_accept(tb_app *t, void *data) {
    tb_filedlg *fd = data;
    if (fd->save) {
        const char *nm = tb_field_get(fd->name);
        if (!nm[0]) return;
        tb_fs_join(fd->dir, nm, fd->result, sizeof fd->result);
    } else {
        tb_node *n = tb_view_selected(fd->view);
        if (!n || n->expandable) return;
        tb_fs_join(fd->dir, n->cells[0], fd->result, sizeof fd->result);
    }
    tb_dialog_close(t, fd->d, 1);
}

static inline void tb_fd_up(tb_app *t, void *data) {
    tb_filedlg *fd = data;
    char p[TB_PATH_MAX];
    snprintf(p, sizeof p, "%s", fd->dir);
    if (tb_fs_parent(p)) tb_fd_load(t, fd, p);
}

/* Double-click or Return in the list. */
static inline void tb_fd_open(tb_app *t, void *data) {
    tb_filedlg *fd = data;
    tb_node *n = tb_view_selected(fd->view);
    if (!n) return;
    if (n->expandable) {
        char p[2 * TB_PATH_MAX];
        tb_fs_join(fd->dir, n->cells[0], p, sizeof p);
        tb_fd_load(t, fd, p);
        return;
    }
    if (fd->save) tb_field_set(fd->name, n->cells[0]);
    tb_fd_accept(t, fd);
}

/* Selection changed: in Save As, copy the file name into the field. */
static inline void tb_fd_select(tb_app *t, void *data) {
    (void)t;
    tb_filedlg *fd = data;
    tb_node *n = tb_view_selected(fd->view);
    if (fd->save && n && !n->expandable) tb_field_set(fd->name, n->cells[0]);
}

static inline int tb_filedlg_run(tb_app *t, const char *title, const char *start,
                                 int save, const char *defname, char *out, size_t cap) {
    char dir[TB_PATH_MAX];
    if (!start || !start[0]) start = ".";
    if (!realpath(start, dir)) snprintf(dir, sizeof dir, "%s", start);

    const int W = TB_FD_W, H = TB_FD_H;
    int X = (t->width - W) / 2, Y = (t->height - H) / 2;
    if (X < 0) X = 0;
    if (Y < 0) Y = 0;

    tb_filedlg *fd = calloc(1, sizeof *fd);
    if (!fd) tb_oom();
    fd->save = save;
    fd->pathw = W - 70 - 80;
    fd->d = tb_dialog_new(t, X, Y, W, H, title);

    tb_container *prev = tb_build_begin(t, &fd->d->c);
    tb_label_new(t, X + 14, Y + 30, TB_BLACK, "Folder:");
    fd->pathlbl = tb_label_new(t, X + 70, Y + 30, TB_NAVY, "");
    tb_button_new(t, X + W - 14 - 64, Y + 26, 64, 20, "Up", tb_fd_up, fd);

    const char *titles[3] = { "Name", "Size", "Kind" };
    const int widths[3] = { 166, 68, 100 };
    fd->view = tb_list_new(t, X + 14, Y + 52, W - 28, 170, 3, titles, widths);
    tb_view_folders_first(fd->view, 1);
    tb_view_sort(fd->view, 0, 0);
    tb_view_on_select(fd->view, tb_fd_select, fd);
    tb_view_on_open(fd->view, tb_fd_open, fd);

    if (save) {
        tb_label_new(t, X + 14, Y + 231, TB_BLACK, "Name:");
        fd->name = tb_field_new(t, X + 60, Y + 228, W - 74, 22);
        tb_field_set(fd->name, defname ? defname : "");
        tb_field_on_accept(fd->name, tb_fd_accept, fd);
    }

    int by = Y + H - 14 - 24;
    tb_dialog_button(t, X + W - 14 - 80 - 10 - 80, by, 80, 24, "Cancel", 0);
    tb_button *ok = tb_button_new(t, X + W - 14 - 80, by, 80, 24,
                                  save ? "Save" : "Open", tb_fd_accept, fd);
    tb_dialog_set_default(fd->d, ok);
    tb_dialog_set_cancel(fd->d, 0);
    tb_build_end(t, prev);

    fd->d->c.focus = save ? &fd->name->base : &fd->view->base;
    tb_fd_load(t, fd, dir);

    int r = tb_dialog_run(t, fd->d);
    int chosen = (r == 1 && fd->result[0] && out && strlen(fd->result) < cap);
    if (chosen) strcpy(out, fd->result);

    tb_dialog_free(fd->d);
    free(fd);
    return chosen;
}

static inline int tb_open_dialog(tb_app *t, const char *start, char *out, size_t cap) {
    return tb_filedlg_run(t, "Open", start, 0, NULL, out, cap);
}

static inline int tb_save_dialog(tb_app *t, const char *start, const char *name,
                                 char *out, size_t cap) {
    return tb_filedlg_run(t, "Save As", start, 1, name, out, cap);
}

#endif /* TB_FILEDLG_H */
