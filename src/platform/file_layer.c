#include "file_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_OPEN 16

typedef struct { uint32_t lba, bytes, word; } TableRec;

struct JojoFileLayer {
    char *root;
    JojoFileInfo *files; size_t nfiles;
    size_t *by_lba;              /* file numbers sorted by lba (form 1 only) */
    size_t nby;
    TableRec *table; size_t ntable;
    int *table_file;             /* table index -> file number or -1 */
    struct { size_t file; FILE *fp; unsigned long stamp; } open_[MAX_OPEN];
    unsigned long clock_;
};

static char g_err[256];
static void seterr(const char *a, const char *b) { snprintf(g_err, sizeof g_err, "%s%s", a, b ? b : ""); }
const char *jfl_error(void) { return g_err; }

static JojoFileInfo *g_sort_files;
static int cmp_by_lba(const void *pa, const void *pb) {
    uint32_t a = g_sort_files[*(const size_t *)pa].lba, b = g_sort_files[*(const size_t *)pb].lba;
    return a < b ? -1 : a > b;
}

JojoFileLayer *jfl_open(const char *root_dir) {
    size_t n = strlen(root_dir);
    char *idx = malloc(n + 16);
    if (!idx) { seterr("out of memory", NULL); return NULL; }
    snprintf(idx, n + 16, "%s/index.tsv", root_dir);
    FILE *f = fopen(idx, "r");
    if (!f) { seterr("cannot open ", idx); free(idx); return NULL; }
    free(idx);
    JojoFileLayer *l = calloc(1, sizeof *l);
    l->root = strdup(root_dir);
    size_t capf = 64, capt = 64;
    l->files = malloc(capf * sizeof *l->files);
    l->table = malloc(capt * sizeof *l->table);
    char line[1024];
    while (fgets(line, sizeof line, f)) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = 0;
        if (!len || line[0] == '#') continue;
        char *tab[6]; int nt = 0; char *p = line;
        tab[nt++] = p;
        while (*p && nt < 6) { if (*p == '\t') { *p = 0; tab[nt++] = p + 1; } p++; }
        if (!strcmp(tab[0], "file") && nt == 5) {
            if (l->nfiles == capf) { capf *= 2; l->files = realloc(l->files, capf * sizeof *l->files); }
            JojoFileInfo *fi = &l->files[l->nfiles++];
            fi->path = strdup(tab[1]); fi->lba = (uint32_t)strtoul(tab[2], NULL, 10);
            fi->bytes = (uint32_t)strtoul(tab[3], NULL, 10); fi->form = atoi(tab[4]);
        } else if (!strcmp(tab[0], "table") && nt == 5) {
            size_t i = (size_t)strtoul(tab[1], NULL, 10);
            while (i >= capt) { capt *= 2; l->table = realloc(l->table, capt * sizeof *l->table); }
            if (i >= l->ntable) { memset(l->table + l->ntable, 0, (i + 1 - l->ntable) * sizeof *l->table); l->ntable = i + 1; }
            l->table[i].lba = (uint32_t)strtoul(tab[2], NULL, 10);
            l->table[i].bytes = (uint32_t)strtoul(tab[3], NULL, 10);
            l->table[i].word = (uint32_t)strtoul(tab[4], NULL, 10);
        } else { seterr("bad index line: ", line); fclose(f); jfl_close(l); return NULL; }
    }
    fclose(f);
    if (!l->nfiles) { seterr("index has no files", NULL); jfl_close(l); return NULL; }
    l->by_lba = malloc(l->nfiles * sizeof *l->by_lba);
    for (size_t i = 0; i < l->nfiles; i++) if (l->files[i].form == 1) l->by_lba[l->nby++] = i;
    g_sort_files = l->files; qsort(l->by_lba, l->nby, sizeof *l->by_lba, cmp_by_lba);
    l->table_file = malloc((l->ntable ? l->ntable : 1) * sizeof *l->table_file);
    for (size_t t = 0; t < l->ntable; t++) {
        int found = -1;
        size_t lo = 0, hi = l->nby;
        while (lo < hi) {            /* first entry whose lba >= table lba */
            size_t mid = (lo + hi) / 2;
            if (l->files[l->by_lba[mid]].lba < l->table[t].lba) lo = mid + 1; else hi = mid;
        }
        if (lo < l->nby && l->files[l->by_lba[lo]].lba == l->table[t].lba && l->files[l->by_lba[lo]].bytes == l->table[t].bytes)
            found = (int)l->by_lba[lo];
        l->table_file[t] = found;
    }
    return l;
}

void jfl_close(JojoFileLayer *l) {
    if (!l) return;
    for (int i = 0; i < MAX_OPEN; i++) if (l->open_[i].fp) fclose(l->open_[i].fp);
    for (size_t i = 0; i < l->nfiles; i++) free((void *)l->files[i].path);
    free(l->files); free(l->by_lba); free(l->table); free(l->table_file); free(l->root); free(l);
}

size_t jfl_file_count(const JojoFileLayer *l) { return l->nfiles; }
const JojoFileInfo *jfl_file(const JojoFileLayer *l, size_t i) { return i < l->nfiles ? &l->files[i] : NULL; }
size_t jfl_table_count(const JojoFileLayer *l) { return l->ntable; }
int jfl_table_file(const JojoFileLayer *l, size_t t) { return t < l->ntable ? l->table_file[t] : -1; }

static FILE *open_file(JojoFileLayer *l, size_t fi) {
    int free_slot = -1, lru = 0;
    for (int i = 0; i < MAX_OPEN; i++) {
        if (l->open_[i].fp && l->open_[i].file == fi) { l->open_[i].stamp = ++l->clock_; return l->open_[i].fp; }
        if (!l->open_[i].fp) free_slot = i;
        else if (l->open_[i].stamp < l->open_[lru].stamp) lru = i;
    }
    int slot = free_slot >= 0 ? free_slot : lru;
    if (l->open_[slot].fp) fclose(l->open_[slot].fp);
    size_t n = strlen(l->root) + strlen(l->files[fi].path) + 2;
    char *path = malloc(n);
    snprintf(path, n, "%s/%s", l->root, l->files[fi].path);
    FILE *fp = fopen(path, "rb");
    if (!fp) seterr("cannot open ", path);
    free(path);
    l->open_[slot].fp = fp; l->open_[slot].file = fi; l->open_[slot].stamp = ++l->clock_;
    return fp;
}

long jfl_read_file(JojoFileLayer *l, size_t fi, uint64_t offset, void *dst, size_t len) {
    if (fi >= l->nfiles || l->files[fi].form != 1) { seterr("not a readable file", NULL); return -1; }
    if (offset >= l->files[fi].bytes) return 0;
    if (len > l->files[fi].bytes - offset) len = l->files[fi].bytes - (size_t)offset;
    FILE *fp = open_file(l, fi);
    if (!fp) return -1;
    if (fseek(fp, (long)offset, SEEK_SET) != 0) { seterr("seek failed", NULL); return -1; }
    size_t got = fread(dst, 1, len, fp);
    if (got != len) { seterr("short read", NULL); return -1; }
    return (long)got;
}

long jfl_read_index(JojoFileLayer *l, size_t t, uint64_t offset, void *dst, size_t len) {
    int fi = jfl_table_file(l, t);
    if (fi < 0) { seterr("unknown file index", NULL); return -1; }
    return jfl_read_file(l, (size_t)fi, offset, dst, len);
}

int jfl_find_lba(const JojoFileLayer *l, uint32_t lba, size_t *file_idx, uint32_t *sector_in_file) {
    size_t lo = 0, hi = l->nby;
    while (lo < hi) {                /* last entry whose lba <= target */
        size_t mid = (lo + hi) / 2;
        if (l->files[l->by_lba[mid]].lba <= lba) lo = mid + 1; else hi = mid;
    }
    if (!lo) return 0;
    size_t fi = l->by_lba[lo - 1];
    uint32_t sectors = (l->files[fi].bytes + JFL_SECTOR_BYTES - 1) / JFL_SECTOR_BYTES;
    if (lba - l->files[fi].lba >= sectors) return 0;
    if (file_idx) *file_idx = fi;
    if (sector_in_file) *sector_in_file = lba - l->files[fi].lba;
    return 1;
}

int jfl_lba_form(const JojoFileLayer *l, uint32_t lba) {
    for (size_t i = 0; i < l->nfiles; i++) {          /* linear: only used on the fallback path */
        uint32_t sectors = (l->files[i].bytes + JFL_SECTOR_BYTES - 1) / JFL_SECTOR_BYTES;
        if (lba >= l->files[i].lba && lba - l->files[i].lba < sectors) return l->files[i].form;
    }
    return 0;
}

int jfl_read_sector(JojoFileLayer *l, uint32_t lba, uint8_t out[JFL_SECTOR_BYTES]) {
    size_t fi; uint32_t sec;
    if (!jfl_find_lba(l, lba, &fi, &sec)) return 0;
    memset(out, 0, JFL_SECTOR_BYTES);
    long got = jfl_read_file(l, fi, (uint64_t)sec * JFL_SECTOR_BYTES, out, JFL_SECTOR_BYTES);
    return got >= 0;
}

int jfl_lba_to_table_index(const JojoFileLayer *l, uint32_t lba) {
    size_t fi;
    if (!jfl_find_lba(l, lba, &fi, NULL)) return -1;
    for (size_t t = 0; t < l->ntable; t++) if (l->table_file[t] == (int)fi) return (int)t;
    return -1;
}
