#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "toolbox.h"

/* A small TextEdit clone used to exercise the scrollbars. */

#define DEMO_W 520
#define DEMO_H 340

static tb_textview *editor;
static tb_scrollbar *hbar;
static tb_label *hbar_label;
static const char *filename = "untitled.txt";

static void update_status(tb_app *app, void *data) {
    char buf[64];
    (void)data;
    snprintf(buf, sizeof buf, "Scroll: %d px", tb_scrollbar_get(hbar));
    tb_label_set(app, hbar_label, buf);
}

static int load_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return 0; }
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = 0;
    fclose(f);
    tb_textview_set(editor, buf);
    free(buf);
    return 1;
}

static void file_new(tb_app *app, void *data) {
    (void)app; (void)data;
    tb_textview_set(editor, "");
}

static void file_open(tb_app *app, void *data) {
    (void)app; (void)data;
    load_file(filename);
}

static void file_save(tb_app *app, void *data) {
    (void)app; (void)data;
    char *s = tb_textview_get(editor);
    FILE *f = fopen(filename, "wb");
    if (f) {
        fputs(s, f);
        fclose(f);
    }
    free(s);
}

static void file_quit(tb_app *app, void *data) {
    (void)data;
    tb_quit(app);
}

static void edit_fill(tb_app *app, void *data) {
    char *buf = malloc(200 * 128);
    size_t pos = 0;
    (void)app; (void)data;
    if (!buf) return;
    for (int i = 1; i <= 200; i++)
        pos += (size_t)sprintf(buf + pos,
            "Line %3d: The quick brown fox jumps over the lazy dog. %s\n",
            i, i % 5 == 0 ? "(long line to test the horizontal bar)" : "");
    tb_textview_set(editor, buf);
    free(buf);
}

int main(int argc, char **argv) {
    if (argc > 1) filename = argv[1];

    tb_app *app = tb_app_new("TextEdit", DEMO_W, DEMO_H);
    if (!app) return 1;

    tb_menu_new(app, "File");
    tb_menu_item(app, "New", file_new, NULL);
    tb_menu_item(app, "Open", file_open, NULL);
    tb_menu_item(app, "Save", file_save, NULL);
    tb_menu_item(app, "-", NULL, NULL);
    tb_menu_item(app, "Quit", file_quit, NULL);

    tb_menu_new(app, "Edit");
    tb_menu_item(app, "Clear", file_new, NULL);
    tb_menu_item(app, "Fill with 200 lines", edit_fill, NULL);

    editor = tb_textview_new(app, 8, 26, DEMO_W - 16, 270);
    load_file(filename);

    hbar = tb_scrollbar_new(app, 8, 306, 340, 16, 0, update_status, NULL);
    tb_scrollbar_set(hbar, 400, 100, 0);
    hbar_label = tb_label_new(app, 356, 306, TB_NAVY, "Scroll: 0 px");

    tb_run(app);
    tb_app_free(app);
    return 0;
}
