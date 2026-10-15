CC     ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -O2
LDLIBS  = -lX11

# Header-only library: every header is a dependency of every demo.
HDRS = config.h toolbox.h tb_core.h tb_doc.h tb_scroll.h tb_menu.h \
       tb_label.h tb_button.h tb_scrollbar.h tb_textview.h \
       tb_view.h tb_list.h tb_tree.h

all: demo textedit filebrowser

demo: main.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ main.c $(LDFLAGS) $(LDLIBS)

textedit: textedit.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ textedit.c $(LDFLAGS) $(LDLIBS)

filebrowser: filebrowser.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ filebrowser.c $(LDFLAGS) $(LDLIBS)

clean:
	rm -f demo textedit filebrowser

.PHONY: all clean
