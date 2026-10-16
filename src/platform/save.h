/* File-backed storage for the two game save payloads.
 *
 * Final-target code: plain C99 and host paths only. The hybrid adapter maps
 * the guest's card names to these two fixed files; guest strings never become
 * filesystem paths. */
#ifndef JOJO_SAVE_H
#define JOJO_SAVE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    PLAT_SAVE_JOJOD = 0,
    PLAT_SAVE_JOJOX = 1,
    PLAT_SAVE_FILE_COUNT = 2
} PlatSaveFile;

#define PLAT_SAVE_MAX_BYTES 0x00100000u

/* `directory` must be an absolute path. NULL uses JOJO_SAVE_DIR, then
 * ~/Library/Application Support/JoJo HFTF. Missing directories are created
 * with owner-only permissions. Returns 0 on success, -1 on error. */
int plat_save_init(const char *directory);
void plat_save_shutdown(void);
const char *plat_save_error(void);

/* Only these exact card names are accepted. Both short host names and the
 * game's long PS1 names map to the same two payload files. */
int plat_save_file_from_card_name(const char *name, PlatSaveFile *out);
const char *plat_save_filename(PlatSaveFile file);

/* Existence returns 1/0 for present/missing, or -1 on an I/O/security error.
 * Size/read/write/remove return 0 on success and -1 on error. Reads require
 * the full requested byte range to exist. Writes replace atomically; a
 * nonzero offset performs a bounded read/modify/write of an existing file. */
int plat_save_exists(PlatSaveFile file);
int plat_save_size(PlatSaveFile file, size_t *out_size);
int plat_save_read(PlatSaveFile file, size_t offset, void *dst, size_t size);
int plat_save_write(PlatSaveFile file, size_t offset, const void *src, size_t size);
int plat_save_remove(PlatSaveFile file);

#endif
