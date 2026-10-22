CC     ?= cc
CFLAGS ?= -std=gnu99 -Wall -Wextra -pedantic -O2
LDLIBS  = -lX11

# Platform setup. Requires GNU make (on OpenBSD: pkg_add gmake, run gmake).
OS := $(shell uname -s)
ifeq ($(OS),OpenBSD)
# OpenBSD installs X11 under /usr/X11R6 (not on the default search paths).
CFLAGS  += -I/usr/X11R6/include
LDFLAGS += -L/usr/X11R6/lib
endif

# Header-only library: every header is a dependency of every demo.
HDRS = config.h toolbox.h tb_core.h tb_clip.h tb_doc.h tb_scroll.h tb_menu.h \
       tb_label.h tb_button.h tb_scrollbar.h tb_textview.h \
       tb_view.h tb_list.h tb_tree.h tb_miller.h tb_split.h \
       tb_check.h tb_radio.h tb_group.h tb_icon.h tb_field.h tb_popup.h \
       tb_dialog.h tb_alert.h tb_scrollwin.h tb_slider.h tb_progress.h \
       tb_spin.h tb_combo.h tb_tabs.h tb_status.h tb_colorgrid.h \
       tb_fs.h tb_filedlg.h

LUA_DIR = lua-5.5.0
LUA_OBJ = $(LUA_DIR)/onelua.o

all: demo 

demo: demo.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ demo.c $(LDFLAGS) $(LDLIBS)

clean:
	rm -f demo 

.PHONY: all clean
