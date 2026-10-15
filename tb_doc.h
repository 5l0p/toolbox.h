#ifndef TB_DOC_H
#define TB_DOC_H

/*
 * tb_doc.h - line-based text buffer with a cursor. No X dependencies,
 * so it can be reused for other text widgets. ASCII-oriented: columns
 * are byte offsets.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char *s;
    int len, cap;
} tb_line;

typedef struct {
    tb_line *lines;
    int n, cap;
    int row, col;         /* cursor */
} tb_doc;

static inline void tb_line_reserve(tb_line *l, int need) {
    if (need <= l->cap) return;
    int nc = l->cap ? l->cap : 16;
    while (nc < need) nc *= 2;
    char *p = realloc(l->s, (size_t)nc);
    if (!p) { fprintf(stderr, "toolbox: out of memory\n"); exit(1); }
    l->s = p;
    l->cap = nc;
}

static inline void tb_line_remove(tb_line *l, int pos) {
    memmove(l->s + pos, l->s + pos + 1, (size_t)(l->len - pos));
    l->len--;
}

static inline void tb_line_append(tb_line *l, const char *src, int n) {
    tb_line_reserve(l, l->len + n + 1);
    memcpy(l->s + l->len, src, (size_t)n);
    l->len += n;
    l->s[l->len] = 0;
}

static inline void tb_doc_insert_line(tb_doc *d, int idx, const char *src, int len) {
    if (d->n == d->cap) {
        int nc = d->cap ? d->cap * 2 : 16;
        tb_line *nl = realloc(d->lines, (size_t)nc * sizeof *nl);
        if (!nl) { fprintf(stderr, "toolbox: out of memory\n"); exit(1); }
        d->lines = nl;
        d->cap = nc;
    }
    memmove(&d->lines[idx + 1], &d->lines[idx], (size_t)(d->n - idx) * sizeof *d->lines);
    d->n++;
    tb_line *l = &d->lines[idx];
    l->s = NULL;
    l->len = 0;
    l->cap = 0;
    tb_line_reserve(l, len + 1);
    memcpy(l->s, src, (size_t)len);
    l->len = len;
    l->s[len] = 0;
}

static inline void tb_doc_remove_line(tb_doc *d, int idx) {
    free(d->lines[idx].s);
    memmove(&d->lines[idx], &d->lines[idx + 1], (size_t)(d->n - idx - 1) * sizeof *d->lines);
    d->n--;
}

static inline void tb_doc_clear(tb_doc *d) {
    while (d->n > 0) tb_doc_remove_line(d, d->n - 1);
    d->row = d->col = 0;
}

static inline void tb_doc_free(tb_doc *d) {
    tb_doc_clear(d);
    free(d->lines);
    d->lines = NULL;
    d->cap = 0;
}

/* Replace the whole buffer with s (split on '\n'). Cursor goes to 0,0. */
static inline void tb_doc_set(tb_doc *d, const char *s) {
    tb_doc_clear(d);
    for (;;) {
        const char *nl = strchr(s, '\n');
        int len = nl ? (int)(nl - s) : (int)strlen(s);
        tb_doc_insert_line(d, d->n, s, len);
        if (!nl) break;
        s = nl + 1;
    }
}

/* Returns a malloc'd copy of the whole buffer, lines joined by '\n'. */
static inline char *tb_doc_get(const tb_doc *d) {
    size_t total = 0;
    for (int i = 0; i < d->n; i++) total += (size_t)d->lines[i].len + 1;
    char *out = malloc(total + 1);
    if (!out) { fprintf(stderr, "toolbox: out of memory\n"); exit(1); }
    char *p = out;
    for (int i = 0; i < d->n; i++) {
        memcpy(p, d->lines[i].s, (size_t)d->lines[i].len);
        p += d->lines[i].len;
        if (i + 1 < d->n) *p++ = '\n';
    }
    *p = 0;
    return out;
}

static inline void tb_doc_insert_char(tb_doc *d, char c) {
    tb_line *l = &d->lines[d->row];
    tb_line_reserve(l, l->len + 2);
    memmove(l->s + d->col + 1, l->s + d->col, (size_t)(l->len - d->col + 1));
    l->s[d->col] = c;
    l->len++;
    d->col++;
}

static inline void tb_doc_newline(tb_doc *d) {
    int oldlen = d->lines[d->row].len;
    /* insert may move the lines array, so re-fetch the line afterwards */
    tb_doc_insert_line(d, d->row + 1, d->lines[d->row].s + d->col, oldlen - d->col);
    tb_line *l = &d->lines[d->row];
    l->len = d->col;
    l->s[d->col] = 0;
    d->row++;
    d->col = 0;
}

static inline void tb_doc_backspace(tb_doc *d) {
    if (d->col > 0) {
        tb_line_remove(&d->lines[d->row], d->col - 1);
        d->col--;
    } else if (d->row > 0) {
        int prevlen = d->lines[d->row - 1].len;
        tb_line_append(&d->lines[d->row - 1], d->lines[d->row].s, d->lines[d->row].len);
        tb_doc_remove_line(d, d->row);
        d->row--;
        d->col = prevlen;
    }
}

static inline void tb_doc_delete(tb_doc *d) {
    tb_line *l = &d->lines[d->row];
    if (d->col < l->len) {
        tb_line_remove(l, d->col);
    } else if (d->row + 1 < d->n) {
        tb_line_append(l, d->lines[d->row + 1].s, d->lines[d->row + 1].len);
        tb_doc_remove_line(d, d->row + 1);
    }
}

static inline void tb_doc_clamp_col(tb_doc *d) {
    int len = d->lines[d->row].len;
    if (d->col > len) d->col = len;
}

static inline void tb_doc_left(tb_doc *d) {
    if (d->col > 0) d->col--;
    else if (d->row > 0) { d->row--; d->col = d->lines[d->row].len; }
}

static inline void tb_doc_right(tb_doc *d) {
    if (d->col < d->lines[d->row].len) d->col++;
    else if (d->row + 1 < d->n) { d->row++; d->col = 0; }
}

static inline void tb_doc_up(tb_doc *d) {
    if (d->row > 0) { d->row--; tb_doc_clamp_col(d); }
}

static inline void tb_doc_down(tb_doc *d) {
    if (d->row + 1 < d->n) { d->row++; tb_doc_clamp_col(d); }
}

static inline void tb_doc_move_rows(tb_doc *d, int delta) {
    d->row += delta;
    if (d->row < 0) d->row = 0;
    if (d->row >= d->n) d->row = d->n - 1;
    tb_doc_clamp_col(d);
}

/* Text between (r1,c1) and (r2,c2), lines joined by '\n'. Caller frees. */
static inline char *tb_doc_range_text(const tb_doc *d, int r1, int c1, int r2, int c2) {
    size_t total = 1;
    for (int r = r1; r <= r2; r++) total += (size_t)d->lines[r].len + 1;
    char *out = malloc(total);
    if (!out) { fprintf(stderr, "toolbox: out of memory\n"); exit(1); }
    char *p = out;
    for (int r = r1; r <= r2; r++) {
        int from = r == r1 ? c1 : 0;
        int to = r == r2 ? c2 : d->lines[r].len;
        if (to > from) {
            memcpy(p, d->lines[r].s + from, (size_t)(to - from));
            p += to - from;
        }
        if (r < r2) *p++ = '\n';
    }
    *p = 0;
    return out;
}

/* Remove the text between (r1,c1) and (r2,c2); the cursor goes to (r1,c1). */
static inline void tb_doc_delete_range(tb_doc *d, int r1, int c1, int r2, int c2) {
    if (r1 == r2) {
        tb_line *l = &d->lines[r1];
        memmove(l->s + c1, l->s + c2, (size_t)(l->len - c2 + 1));
        l->len -= c2 - c1;
    } else {
        tb_line *a = &d->lines[r1];
        tb_line *b = &d->lines[r2];
        int tail = b->len - c2;
        a->len = c1;
        a->s[c1] = 0;
        tb_line_append(a, b->s + c2, tail);
        for (int r = r2; r > r1; r--) tb_doc_remove_line(d, r);
    }
    d->row = r1;
    d->col = c1;
}

#endif /* TB_DOC_H */
