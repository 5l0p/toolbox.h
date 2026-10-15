#ifndef TB_ALERT_H
#define TB_ALERT_H

/*
 * tb_alert.h - System 6 style alert box: an icon, a word-wrapped message
 * and one to three buttons. The first button is the default (Return).
 * Escape selects the last button.
 *
 *   int r = tb_alert(app, TB_ALERT_CAUTION, "Discard changes?",
 *                    "Discard", "Cancel", NULL);      // returns 1 or 2
 */

#include "tb_dialog.h"

enum { TB_ALERT_STOP, TB_ALERT_NOTE, TB_ALERT_CAUTION };

#define TB_ALERT_ICON 32

/* Icons are drawn with polygons and arcs, so no bitmaps are needed. */
static inline void tb_alert_icon(tb_app *t, int kind, int x, int y) {
    int base = tb_text_base(t, y, TB_ALERT_ICON);
    if (kind == TB_ALERT_STOP) {
        static const short ox[8] = { 10, 22, 32, 32, 22, 10, 0, 0 };
        static const short oy[8] = {  0,  0, 10, 22, 32, 32, 22, 10 };
        XPoint p[8];
        for (int i = 0; i < 8; i++) { p[i].x = (short)(x + ox[i]); p[i].y = (short)(y + oy[i]); }
        tb_poly(t, p, 8, TB_C_FG);
        tb_str(t, x + 13, base, TB_C_BG, "!");
    } else if (kind == TB_ALERT_NOTE) {
        tb_arc(t, x, y, TB_ALERT_ICON, TB_ALERT_ICON, TB_C_FG, 1);
        tb_str(t, x + 12, base, TB_C_BG, "?");
    } else {
        XPoint p[3];
        p[0].x = (short)(x + 16); p[0].y = (short)y;
        p[1].x = (short)(x + 32); p[1].y = (short)(y + 30);
        p[2].x = (short)x;        p[2].y = (short)(y + 30);
        tb_poly(t, p, 3, TB_C_FG);
        tb_str(t, x + 13, y + 27, TB_C_BG, "!");
    }
}

/* Message text plus the alert icon (drawn to the left of the text). */
typedef struct tb_msg {
    tb_widget base;
    int kind;             /* -1 = no icon */
    char **lines;
    int nl;
} tb_msg;

static inline void tb_msg_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_msg *m = (tb_msg *)wb;
    (void)focused;
    if (m->kind >= 0) tb_alert_icon(t, m->kind, wb->x - 48, wb->y);
    int lh = t->fascent + t->fdescent;
    for (int i = 0; i < m->nl; i++)
        tb_str(t, wb->x, wb->y + i * lh + t->fascent, TB_C_FG, m->lines[i]);
}

static inline void tb_msg_destroy(tb_widget *wb) {
    tb_msg *m = (tb_msg *)wb;
    for (int i = 0; i < m->nl; i++) free(m->lines[i]);
    free(m->lines);
    free(m);
}

static const tb_wops TB_MSG_OPS = {
    tb_msg_draw, NULL, NULL, NULL, NULL, NULL, tb_msg_destroy, NULL
};

static inline void tb_msg_new(tb_app *t, int x, int y, int w, int kind,
                              char **lines, int nl) {
    tb_msg *m = calloc(1, sizeof *m);
    if (!m) tb_oom();
    m->base.ops = &TB_MSG_OPS;
    m->base.x = x;
    m->base.y = y;
    m->base.w = w;
    m->base.h = (t->fascent + t->fdescent) * nl;
    m->kind = kind;
    m->lines = lines;
    m->nl = nl;
    tb_add_widget(t, &m->base);
}

/* Greedy word wrap of s into lines no wider than maxw pixels.
 * Returns the number of lines; *out receives malloc'd strings. */
static inline int tb_wrap(tb_app *t, const char *s, int maxw, char ***out) {
    int cap = 8, n = 0;
    char **lines = malloc((size_t)cap * sizeof *lines);
    if (!lines) tb_oom();
    char cur[512];
    cur[0] = 0;
    const char *p = s;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        const char *q = p;
        while (*q && *q != ' ') q++;
        char word[256];
        size_t wl = (size_t)(q - p);
        if (wl >= sizeof word) wl = sizeof word - 1;
        memcpy(word, p, wl);
        word[wl] = 0;

        char cand[512];
        if (cur[0]) snprintf(cand, sizeof cand, "%s %s", cur, word);
        else snprintf(cand, sizeof cand, "%s", word);

        if (cur[0] && tb_text_w(t, cand) > maxw) {
            if (n == cap) {
                cap *= 2;
                lines = realloc(lines, (size_t)cap * sizeof *lines);
                if (!lines) tb_oom();
            }
            lines[n++] = tb_strdup(cur);
            snprintf(cur, sizeof cur, "%s", word);
        } else {
            snprintf(cur, sizeof cur, "%s", cand);
        }
        p = q;
    }
    if (cur[0] || n == 0) {
        if (n == cap) {
            cap *= 2;
            lines = realloc(lines, (size_t)cap * sizeof *lines);
            if (!lines) tb_oom();
        }
        lines[n++] = tb_strdup(cur);
    }
    *out = lines;
    return n;
}

/* Show a modal alert. Returns 1..3 for the button pressed (0 if the
 * window was closed). */
static inline int tb_alert(tb_app *t, int kind, const char *msg,
                           const char *b1, const char *b2, const char *b3) {
    const char *labels[3] = { b1, b2, b3 };
    int nb = 0;
    while (nb < 3 && labels[nb]) nb++;
    if (nb == 0) { labels[0] = "OK"; nb = 1; }

    const int W = 340, TX = 64, BW = 80, BH = 24, GAP = 10;
    int maxw = W - TX - 16;
    char **lines;
    int nl = tb_wrap(t, msg, maxw, &lines);
    int lh = t->fascent + t->fdescent;
    int body = nl * lh > TB_ALERT_ICON ? nl * lh : TB_ALERT_ICON;
    int H = 16 + body + 20 + BH + 16;

    int X = (t->width - W) / 2, Y = (t->height - H) / 2;
    if (X < 0) X = 0;
    if (Y < 0) Y = 0;

    tb_dialog *d = tb_dialog_new(t, X, Y, W, H, "");
    tb_container *prev = tb_build_begin(t, &d->c);
    tb_msg_new(t, X + TX, Y + 16, maxw, kind, lines, nl);
    int bx = X + W - 16 - nb * BW - (nb - 1) * GAP;
    int by = Y + H - 16 - BH;
    for (int i = 0; i < nb; i++) {
        tb_button *b = tb_dialog_button(t, bx + i * (BW + GAP), by, BW, BH,
                                        labels[i], i + 1);
        if (i == 0) tb_dialog_set_default(d, b);
    }
    tb_dialog_set_cancel(d, nb);
    tb_build_end(t, prev);

    int r = tb_dialog_run(t, d);
    tb_dialog_free(d);
    return r < 0 ? 0 : r;
}

#endif /* TB_ALERT_H */
