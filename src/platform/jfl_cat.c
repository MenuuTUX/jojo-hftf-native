/* Test tool: jfl_cat <root> <table_index> [offset len]   writes file bytes to stdout.   jfl_cat <root> lba <lba>  writes one sector. */
#include "file_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage\n"); return 2; }
    JojoFileLayer *l = jfl_open(argv[1]);
    if (!l) { fprintf(stderr, "%s\n", jfl_error()); return 1; }
    if (!strcmp(argv[2], "lba")) {
        uint8_t sec[JFL_SECTOR_BYTES];
        if (!jfl_read_sector(l, (uint32_t)strtoul(argv[3], NULL, 10), sec)) return 3;
        fwrite(sec, 1, sizeof sec, stdout); return 0;
    }
    size_t t = (size_t)strtoul(argv[2], NULL, 10);
    int fi = jfl_table_file(l, t);
    if (fi < 0) { fprintf(stderr, "no file for index %zu\n", t); return 3; }
    uint64_t off = argc > 3 ? strtoull(argv[3], NULL, 10) : 0;
    size_t len = argc > 4 ? (size_t)strtoull(argv[4], NULL, 10) : jfl_file(l, (size_t)fi)->bytes;
    void *buf = malloc(len ? len : 1);
    long got = jfl_read_index(l, t, off, buf, len);
    if (got < 0) { fprintf(stderr, "%s\n", jfl_error()); return 4; }
    fwrite(buf, 1, (size_t)got, stdout);
    return 0;
}
