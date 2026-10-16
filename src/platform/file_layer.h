/* File layer for the native JoJo port: game data files by index, served from a folder extracted from the user's own disc.
 *
 * Final-target code: plain C99, no PS1 types, no emulator. The folder and its index.tsv are produced at build time by
 * tools-phase1/extract_disc.py. Single-threaded; callers serialise access.
 *
 * Two ways to address data:
 *   - by game file index: the 1060-record table the original executable keeps at 0x8005631C. jfl_read_index() is what
 *     the rewritten loader calls.
 *   - by disc sector (LBA): only for the hybrid stage, where the original libcd still asks for sectors. */
#ifndef JOJO_FILE_LAYER_H
#define JOJO_FILE_LAYER_H
#include <stddef.h>
#include <stdint.h>

#define JFL_SECTOR_BYTES 2048

typedef struct JojoFileLayer JojoFileLayer;

typedef struct {
    const char *path;     /* path relative to the folder, e.g. "M/AC_MAIN.BIN" */
    uint32_t lba;         /* first sector on the original disc */
    uint32_t bytes;       /* file size */
    int form;             /* 1 = copied to the folder, 2 = XA audio, listed but not copied */
} JojoFileInfo;

/* Opens <root_dir>/index.tsv. Returns NULL on failure; jfl_error() then describes it. */
JojoFileLayer *jfl_open(const char *root_dir);
void jfl_close(JojoFileLayer *l);
const char *jfl_error(void);

size_t jfl_file_count(const JojoFileLayer *l);
const JojoFileInfo *jfl_file(const JojoFileLayer *l, size_t file_idx);

/* Game file index (0..jfl_table_count()-1) -> file number, or -1 when the index is unknown or points at a missing file. */
size_t jfl_table_count(const JojoFileLayer *l);
int jfl_table_file(const JojoFileLayer *l, size_t table_index);

/* Reads up to len bytes at offset. Returns the byte count (short at end of file) or -1 on error. Form 2 files fail. */
long jfl_read_file(JojoFileLayer *l, size_t file_idx, uint64_t offset, void *dst, size_t len);
long jfl_read_index(JojoFileLayer *l, size_t table_index, uint64_t offset, void *dst, size_t len);

/* Hybrid stage: sector lookup. Returns 1 and fills the outputs when lba lies inside a form 1 file extent. */
int jfl_find_lba(const JojoFileLayer *l, uint32_t lba, size_t *file_idx, uint32_t *sector_in_file);
/* 0 when lba is outside every listed file, else the form (1 or 2) of the file that contains it. */
int jfl_lba_form(const JojoFileLayer *l, uint32_t lba);
/* Reads one 2048-byte sector of a form 1 file (the tail of the last sector is zero filled). Returns 1 on success. */
int jfl_read_sector(JojoFileLayer *l, uint32_t lba, uint8_t out[JFL_SECTOR_BYTES]);
/* Table index of the file that owns lba, or -1. */
int jfl_lba_to_table_index(const JojoFileLayer *l, uint32_t lba);

#endif
