/* H1 hybrid glue, piece B3 (file layer). DEV STAGE ONLY: this file adapts the native file layer (platform/file_layer.c) to the
 * PS1 CD controller of the psxrecomp runtime. It is not part of the final target (see PHASE1_DESIGN.md, section 6).
 *
 * The runtime asks psx_cd_sector_override() for every sector the game's libcd reads. Sectors that belong to a form 1 file in the
 * extracted folder are built here from the folder; everything else (XA audio, ISO structures, padding) falls back to the disc image.
 *
 * Environment:
 *   (unset) or 0             off: all sectors from the disc image, the REFERENCE (H0) behavior. Off is the default.
 *   JOJO_FILE_LAYER=1        serve form 1 files from the folder
 *   JOJO_FILE_LAYER=verify   serve from the folder AND compare each sector with the disc image (counts mismatches)
 *   JOJO_EXTRACTED_DIR=...   folder produced by tools-phase1/extract_disc.py (default: compiled in, see CMake) */
#include "file_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

extern int iso_read_raw_sector(void *handle, uint32_t lba, uint8_t *buffer, int size);

#ifndef JOJO_EXTRACTED_DIR_DEFAULT
#define JOJO_EXTRACTED_DIR_DEFAULT "extracted"
#endif

static JojoFileLayer *g_layer;
static int g_state;                    /* 0 not initialised, 1 serving, 2 disabled, 3 failed */
static int g_verify;
static uint64_t g_served, g_fallback, g_verified, g_mismatch, g_next_report = 5000;
static uint64_t g_fallback_in_files;   /* fallbacks for sectors that belong to a form 1 file: must stay 0 */
static uint64_t g_fallback_xa, g_fallback_other;   /* form 2 (XA audio) files, and sectors outside every file */
static uint32_t g_first_other[8]; static int g_n_first_other;

static uint8_t bcd(unsigned v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

static void report(const char *tag) {
    fprintf(stderr, "psx-file-layer[%s]: served %llu from folder, %llu fell back to the disc image (%llu inside form 1 files, %llu XA audio, %llu outside every file), verified %llu, mismatches %llu\n",
            tag, (unsigned long long)g_served, (unsigned long long)g_fallback, (unsigned long long)g_fallback_in_files,
            (unsigned long long)g_fallback_xa, (unsigned long long)g_fallback_other, (unsigned long long)g_verified, (unsigned long long)g_mismatch);
    if (g_n_first_other) { fprintf(stderr, "psx-file-layer[%s]: first lbas outside every file:", tag); for (int i = 0; i < g_n_first_other; i++) fprintf(stderr, " %u", g_first_other[i]); fprintf(stderr, "\n"); }
}
static void report_exit(void) { report("exit"); }

static void init(void) {
    const char *mode = getenv("JOJO_FILE_LAYER");
    if (!mode || !*mode || !strcmp(mode, "0")) { g_state = 2; fprintf(stderr, "psx-file-layer: off (set JOJO_FILE_LAYER=1 or verify to enable), sectors come from the disc image\n"); return; }
    g_verify = mode && !strcmp(mode, "verify");
    const char *dir = getenv("JOJO_EXTRACTED_DIR");
    if (!dir || !*dir) dir = JOJO_EXTRACTED_DIR_DEFAULT;
    g_layer = jfl_open(dir);
    if (!g_layer) { g_state = 3; fprintf(stderr, "psx-file-layer: cannot open %s: %s (falling back to the disc image)\n", dir, jfl_error()); return; }
    g_state = 1;
    atexit(report_exit);
    fprintf(stderr, "psx-file-layer: serving form 1 files from %s (%zu files, %zu table records)%s\n", dir, jfl_file_count(g_layer), jfl_table_count(g_layer), g_verify ? ", verify mode" : "");
}

int psx_cd_sector_override(void *iso_handle, uint32_t lba, uint8_t *raw) {
    if (g_state == 0) init();
    if (g_state != 1) return 0;
    size_t fi; uint32_t sec;
    if (!jfl_find_lba(g_layer, lba, &fi, &sec)) {
        g_fallback++;
        if (jfl_lba_form(g_layer, lba) == 2) g_fallback_xa++;
        else { g_fallback_other++; if (g_n_first_other < 8) g_first_other[g_n_first_other++] = lba; }
        return 0;
    }
    uint8_t data[JFL_SECTOR_BYTES];
    if (!jfl_read_sector(g_layer, lba, data)) { g_fallback++; g_fallback_in_files++; return 0; }
    const JojoFileInfo *f = jfl_file(g_layer, fi);
    uint32_t sectors = (f->bytes + JFL_SECTOR_BYTES - 1) / JFL_SECTOR_BYTES;
    unsigned submode = (sec + 1 == sectors) ? 0x89 : 0x08;      /* data; EOF and EOR on the last sector (the only two values on this disc) */
    uint32_t abs = lba + 150; unsigned m = abs / (60 * 75), s = (abs / 75) % 60, fr = abs % 75;
    memset(raw, 0, 2352);
    raw[0] = 0x00; memset(raw + 1, 0xFF, 10); raw[11] = 0x00;
    raw[12] = bcd(m); raw[13] = bcd(s); raw[14] = bcd(fr); raw[15] = 2;
    raw[16] = 0; raw[17] = 0; raw[18] = (uint8_t)submode; raw[19] = 0;
    memcpy(raw + 20, raw + 16, 4);
    memcpy(raw + 24, data, JFL_SECTOR_BYTES);                    /* EDC/ECC (raw + 2072) is not generated: the controller model does not read it */
    g_served++;
    if (g_verify) {
        uint8_t ref[2352];
        if (iso_read_raw_sector(iso_handle, lba, ref, 2352)) {
            g_verified++;
            if (memcmp(ref, raw, 2072) != 0) {
                if (g_mismatch < 8) fprintf(stderr, "psx-file-layer: MISMATCH at lba %u (file %s, sector %u)\n", lba, f->path, sec);
                g_mismatch++;
            }
        }
    }
    if (g_served >= g_next_report) { g_next_report += 5000; report("progress"); }
    return 1;
}
