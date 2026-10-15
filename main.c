#include <stdio.h>
#include "config.h"
#include "toolbox.h"

static void say(tb_app *app, void *data) {
    (void)app;
    printf("%s\n", (const char *)data);
    fflush(stdout);
}

static void quit_cb(tb_app *app, void *data) {
    (void)data;
    tb_quit(app);
}

int main(void) {
    tb_app *app = tb_app_new(TB_WIN_TITLE, TB_WIN_WIDTH, TB_WIN_HEIGHT);
    if (!app) return 1;

    tb_menu_new(app, "Apple");
    tb_menu_item(app, "About Toolbox...", say, "About");

    tb_menu_new(app, "File");
    tb_menu_item(app, "New", say, "File>New");
    tb_menu_item(app, "Open...", say, "File>Open");
    tb_menu_item(app, "-", NULL, NULL);
    tb_menu_item(app, "Quit", quit_cb, NULL);

    tb_menu_new(app, "Edit");
    tb_menu_item(app, "Cut", say, "Edit>Cut");
    tb_menu_item(app, "Copy", say, "Edit>Copy");
    tb_menu_item(app, "Paste", say, "Edit>Paste");

    tb_label_new(app, 16, 40, TB_NAVY, "Hello from the toolbox!");
    tb_label_new(app, 16, 60, TB_RED, "16-colour palette from config.h");
    tb_button_new(app, 16, 90, 90, 24, "OK", say, "OK");
    tb_button_new(app, 120, 90, 90, 24, "Cancel", say, "Cancel");

    tb_run(app);
    tb_app_free(app);
    return 0;
}
