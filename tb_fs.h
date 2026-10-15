#ifndef TB_FS_H
#define TB_FS_H

/*
 * tb_fs.h - directory listing and path helpers for the file dialogs.
 * Uses dirent.h and sys/stat.h, so build with -std=gnu99 (or a POSIX
 * feature macro) rather than strict -std=c99.
 */

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "tb_core.h"

#ifndef TB_PATH_MAX
#define TB_PATH_MAX 4096
#endif

typedef struct {
    char *name;
    int isdir;
    long long size;
} tb_fs_entry;

/* out = dir + "/" + name, without doubling the slash. */
static inline void tb_fs_join(const char *dir, const char *name, char *out, size_t cap) {
    size_t l = strlen(dir);
    if (l > 0 && dir[l - 1] == '/') snprintf(out, cap, "%s%s", dir, name);
    else snprintf(out, cap, "%s/%s", dir, name);
}

/* Remove the last component of path in place. Returns 0 if already at "/". */
static inline int tb_fs_parent(char *path) {
    char *slash = strrchr(path, '/');
    if (!slash) return 0;
    if (slash == path) {
        if (path[1] == 0) return 0;
        path[1] = 0;
        return 1;
    }
    *slash = 0;
    return 1;
}

static inline void tb_fs_format_size(long long n, char *out, size_t cap) {
    if (n < 1024)             snprintf(out, cap, "%lld bytes", n);
    else if (n < 1024 * 1024) snprintf(out, cap, "%.1f KB", n / 1024.0);
    else                      snprintf(out, cap, "%.1f MB", n / (1024.0 * 1024.0));
}

/* "Folder", "Document", or "<EXT> document" from the file name. */
static inline void tb_fs_kind(const char *name, int isdir, char *out, size_t cap) {
    if (isdir) { snprintf(out, cap, "Folder"); return; }
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name || !dot[1]) { snprintf(out, cap, "Document"); return; }
    char ext[32];
    size_t i = 0;
    for (; dot[1 + i] && i + 1 < sizeof ext; i++)
        ext[i] = (char)toupper((unsigned char)dot[1 + i]);
    ext[i] = 0;
    snprintf(out, cap, "%s document", ext);
}

static inline int tb_fs_cmpi(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

/* Folders first, then by name (case-insensitive). */
static inline int tb_fs_cmp(const void *pa, const void *pb) {
    const tb_fs_entry *a = pa, *b = pb;
    if (a->isdir != b->isdir) return b->isdir - a->isdir;
    return tb_fs_cmpi(a->name, b->name);
}

/* Read a directory. Returns a sorted array (free with tb_fs_free), or NULL
 * with *count = 0 if it cannot be read. Hidden files are included. */
static inline tb_fs_entry *tb_fs_list(const char *dir, int *count) {
    *count = 0;
    DIR *d = opendir(dir);
    if (!d) return NULL;
    size_t n = 0, cap = 0;
    tb_fs_entry *e = NULL;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        char full[TB_PATH_MAX];
        tb_fs_join(dir, de->d_name, full, sizeof full);
        struct stat st;
        if (stat(full, &st) != 0) memset(&st, 0, sizeof st);
        if (n == cap) {
            cap = cap ? cap * 2 : 64;
            e = realloc(e, cap * sizeof *e);
            if (!e) tb_oom();
        }
        e[n].name = tb_strdup(de->d_name);
        e[n].isdir = S_ISDIR(st.st_mode) ? 1 : 0;
        e[n].size = (long long)st.st_size;
        n++;
    }
    closedir(d);
    if (n > 0) qsort(e, n, sizeof *e, tb_fs_cmp);
    *count = (int)n;
    return e;
}

static inline void tb_fs_free(tb_fs_entry *e, int n) {
    for (int i = 0; i < n; i++) free(e[i].name);
    free(e);
}

#endif /* TB_FS_H */
