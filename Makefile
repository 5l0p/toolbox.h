CC     ?= cc
CFLAGS ?= -std=gnu99 -Wall -Wextra -pedantic -O2
LDLIBS  = -lX11

# Header-only library: every header is a dependency of every demo.
HDRS = config.h toolbox.h tb_core.h tb_clip.h tb_doc.h tb_scroll.h tb_menu.h \
       tb_label.h tb_button.h tb_scrollbar.h tb_textview.h \
       tb_view.h tb_list.h tb_tree.h \
       tb_check.h tb_radio.h tb_group.h tb_icon.h tb_field.h tb_popup.h \
       tb_dialog.h tb_alert.h tb_scrollwin.h tb_slider.h tb_progress.h \
       tb_spin.h tb_combo.h tb_tabs.h tb_status.h tb_colorgrid.h \
       tb_fs.h tb_filedlg.h

LUA_DIR = lua-5.5.0
LUA_OBJ = $(LUA_DIR)/onelua.o

all: demo textedit filebrowser controls gallery tb_lua

# Lua core and libraries as one object (onelua.c without its main()).
$(LUA_OBJ): $(LUA_DIR)/onelua.c
	$(CC) -O2 -DMAKE_LIB -DLUA_USE_LINUX -c -o $@ $<

tb_lua: tb_lua.c $(HDRS) $(LUA_OBJ)
	$(CC) $(CFLAGS) -I$(LUA_DIR) -o $@ tb_lua.c $(LUA_OBJ) $(LDFLAGS) $(LDLIBS) -lm -ldl

demo: main.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ main.c $(LDFLAGS) $(LDLIBS)

textedit: textedit.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ textedit.c $(LDFLAGS) $(LDLIBS)

filebrowser: filebrowser.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ filebrowser.c $(LDFLAGS) $(LDLIBS)

controls: controls.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ controls.c $(LDFLAGS) $(LDLIBS)

gallery: gallery.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ gallery.c $(LDFLAGS) $(LDLIBS)

clean:
	rm -f demo textedit filebrowser controls gallery tb_lua $(LUA_OBJ)

.PHONY: all clean
