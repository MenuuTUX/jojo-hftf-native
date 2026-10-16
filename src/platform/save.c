#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L

#include "save.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef O_NOFOLLOW
#error "platform/save.c requires O_NOFOLLOW for safe save-file access"
#endif

static const char *const k_save_names[PLAT_SAVE_FILE_COUNT] = {
    "JOJOD",
    "JOJOX"
};

static const char *const k_card_names[PLAT_SAVE_FILE_COUNT] = {
    "BASLUSP01060-JOJOD",
    "BASLUSP01060-JOJOX"
};

static int g_directory_fd = -1;
static char g_directory[PATH_MAX];
static char g_error[256];
static unsigned long g_temp_serial;

static void save_error(const char *message) {
    if (!message) message = "save operation failed";
    (void)snprintf(g_error, sizeof g_error, "%s: %s", message, strerror(errno));
}

static void save_error_text(const char *message) {
    (void)snprintf(g_error, sizeof g_error, "%s", message ? message : "save operation failed");
}

const char *plat_save_error(void) {
    return g_error;
}

const char *plat_save_filename(PlatSaveFile file) {
    if (file < 0 || file >= PLAT_SAVE_FILE_COUNT) return NULL;
    return k_save_names[file];
}

int plat_save_file_from_card_name(const char *name, PlatSaveFile *out) {
    if (!name || !out) return 0;
    for (int i = 0; i < PLAT_SAVE_FILE_COUNT; ++i) {
        if (strcmp(name, k_save_names[i]) == 0 || strcmp(name, k_card_names[i]) == 0) {
            *out = (PlatSaveFile)i;
            return 1;
        }
    }
    return 0;
}

static int make_absolute_path(const char *input, char out[PATH_MAX]) {
    const char *path = input;
    char expanded[PATH_MAX];
    size_t n;

    if (!path || !*path) {
        path = getenv("JOJO_SAVE_DIR");
        if (!path || !*path) {
            const char *home_dir = getenv("HOME");
            if (!home_dir || !*home_dir) {
                save_error_text("HOME is not set and JOJO_SAVE_DIR is empty");
                return 0;
            }
            n = strlen(home_dir);
            if (n + sizeof "/Library/Application Support/JoJo HFTF" > sizeof expanded) {
                save_error_text("default save path is too long");
                return 0;
            }
            (void)snprintf(expanded, sizeof expanded,
                           "%s/Library/Application Support/JoJo HFTF", home_dir);
            path = expanded;
        }
    }

    if (path[0] == '~') {
        if (path[1] != '/' && path[1] != '\0') {
            save_error_text("only a leading ~/ is supported in save paths");
            return 0;
        }
        const char *home_dir = getenv("HOME");
        if (!home_dir || !*home_dir) {
            save_error_text("HOME is not set for a ~/ save path");
            return 0;
        }
        n = strlen(home_dir) + strlen(path + 1);
        if (n >= sizeof expanded) {
            save_error_text("save path is too long");
            return 0;
        }
        (void)snprintf(expanded, sizeof expanded, "%s%s", home_dir, path + 1);
        path = expanded;
    }

    if (path[0] != '/') {
        save_error_text("save directory must be an absolute path");
        return 0;
    }
    n = strlen(path);
    if (n == 0 || n >= PATH_MAX) {
        save_error_text("save path is empty or too long");
        return 0;
    }
    memcpy(out, path, n + 1);
    return 1;
}

static int ensure_directory_tree(char path[PATH_MAX]) {
    size_t n = strlen(path);
    for (size_t i = 1; i <= n; ++i) {
        if (path[i] != '/' && path[i] != '\0') continue;
        char saved = path[i];
        path[i] = '\0';
        struct stat st;
        if (lstat(path, &st) != 0) {
            if (errno != ENOENT || mkdir(path, 0700) != 0) {
                save_error("cannot create save directory");
                path[i] = saved;
                return 0;
            }
            if (lstat(path, &st) != 0) {
                save_error("cannot inspect save directory");
                path[i] = saved;
                return 0;
            }
        }
        if (!S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode)) {
            save_error_text("save path contains a non-directory or symbolic link");
            path[i] = saved;
            return 0;
        }
        path[i] = saved;
    }
    return 1;
}

int plat_save_init(const char *directory) {
    char path[PATH_MAX];
    if (!make_absolute_path(directory, path)) return -1;
    if (g_directory_fd >= 0 && strcmp(path, g_directory) == 0) {
        g_error[0] = '\0';
        return 0;
    }
    if (!ensure_directory_tree(path)) return -1;

    int fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        save_error("cannot open save directory");
        return -1;
    }
    if (g_directory_fd >= 0) (void)close(g_directory_fd);
    g_directory_fd = fd;
    memcpy(g_directory, path, strlen(path) + 1);
    g_error[0] = '\0';
    return 0;
}

void plat_save_shutdown(void) {
    if (g_directory_fd >= 0) (void)close(g_directory_fd);
    g_directory_fd = -1;
    g_directory[0] = '\0';
}

static int valid_file(PlatSaveFile file, const char **name) {
    if (g_directory_fd < 0) {
        save_error_text("save module is not initialized");
        return 0;
    }
    if (file < 0 || file >= PLAT_SAVE_FILE_COUNT) {
        save_error_text("invalid save file id");
        return 0;
    }
    *name = k_save_names[file];
    return 1;
}

static int stat_file(PlatSaveFile file, const char **name, struct stat *st) {
    if (!valid_file(file, name)) return -1;
    if (fstatat(g_directory_fd, *name, st, AT_SYMLINK_NOFOLLOW) != 0) {
        if (errno == ENOENT) return 0;
        save_error("cannot inspect save file");
        return -1;
    }
    if (!S_ISREG(st->st_mode)) {
        save_error_text("save path is not a regular file");
        return -1;
    }
    if (st->st_size < 0 || (uintmax_t)st->st_size > PLAT_SAVE_MAX_BYTES) {
        save_error_text("save file exceeds the supported size");
        return -1;
    }
    return 1;
}

int plat_save_exists(PlatSaveFile file) {
    const char *name;
    struct stat st;
    int rc = stat_file(file, &name, &st);
    if (rc < 0) return -1;
    g_error[0] = '\0';
    return rc;
}

int plat_save_size(PlatSaveFile file, size_t *out_size) {
    const char *name;
    struct stat st;
    if (!out_size) {
        save_error_text("size output is null");
        return -1;
    }
    int rc = stat_file(file, &name, &st);
    if (rc != 1) {
        if (rc == 0) save_error_text("save file does not exist");
        return -1;
    }
    *out_size = (size_t)st.st_size;
    g_error[0] = '\0';
    return 0;
}

static int read_fd_exact(int fd, void *dst, size_t size, off_t offset) {
    unsigned char *bytes = (unsigned char *)dst;
    size_t done = 0;
    while (done < size) {
        ssize_t n = pread(fd, bytes + done, size - done, offset + (off_t)done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return 0;
        done += (size_t)n;
    }
    return 1;
}

int plat_save_read(PlatSaveFile file, size_t offset, void *dst, size_t size) {
    const char *name;
    struct stat st;
    if (size && !dst) {
        save_error_text("read buffer is null");
        return -1;
    }
    if (offset > PLAT_SAVE_MAX_BYTES || size > PLAT_SAVE_MAX_BYTES - offset) {
        save_error_text("save read exceeds the supported size");
        return -1;
    }
    int rc = stat_file(file, &name, &st);
    if (rc != 1) {
        if (rc == 0) save_error_text("save file does not exist");
        return -1;
    }
    if (offset > (size_t)st.st_size || size > (size_t)st.st_size - offset) {
        save_error_text("save read is outside the file payload");
        return -1;
    }
    int fd = openat(g_directory_fd, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        save_error("cannot open save file");
        return -1;
    }
    struct stat opened;
    int ok = fstat(fd, &opened) == 0 && S_ISREG(opened.st_mode) &&
             opened.st_dev == st.st_dev && opened.st_ino == st.st_ino &&
             read_fd_exact(fd, dst, size, (off_t)offset);
    int saved_errno = errno;
    (void)close(fd);
    if (!ok) {
        errno = saved_errno;
        save_error("save read failed or file changed during access");
        return -1;
    }
    g_error[0] = '\0';
    return 0;
}

static int write_fd_exact(int fd, const void *src, size_t size) {
    const unsigned char *bytes = (const unsigned char *)src;
    size_t done = 0;
    while (done < size) {
        ssize_t n = write(fd, bytes + done, size - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return 0;
        done += (size_t)n;
    }
    return 1;
}

static int atomic_replace(const char *name, const void *src, size_t size) {
    struct stat st;
    if (fstatat(g_directory_fd, name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
        if (!S_ISREG(st.st_mode)) {
            save_error_text("refusing to replace a non-regular save path");
            return 0;
        }
    } else if (errno != ENOENT) {
        save_error("cannot inspect save destination");
        return 0;
    }

    char temporary[96];
    int fd = -1;
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
        unsigned long serial = ++g_temp_serial;
        (void)snprintf(temporary, sizeof temporary, ".%s.tmp.%ld.%lu",
                      name, (long)getpid(), serial);
        fd = openat(g_directory_fd, temporary,
                    O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (fd >= 0) break;
        if (errno != EEXIST) {
            save_error("cannot create temporary save file");
            return 0;
        }
    }
    if (fd < 0) {
        save_error_text("could not allocate a unique temporary save file");
        return 0;
    }

    int ok = fchmod(fd, 0600) == 0 && write_fd_exact(fd, src, size) && fsync(fd) == 0;
    int saved_errno = errno;
    if (close(fd) != 0 && ok) {
        ok = 0;
        saved_errno = errno;
    }
    if (!ok) {
        (void)unlinkat(g_directory_fd, temporary, 0);
        errno = saved_errno;
        save_error("cannot write temporary save file");
        return 0;
    }
    if (renameat(g_directory_fd, temporary, g_directory_fd, name) != 0) {
        saved_errno = errno;
        (void)unlinkat(g_directory_fd, temporary, 0);
        errno = saved_errno;
        save_error("cannot atomically replace save file");
        return 0;
    }
    if (fsync(g_directory_fd) != 0) {
        save_error("cannot sync save directory");
        return 0;
    }
    g_error[0] = '\0';
    return 1;
}

int plat_save_write(PlatSaveFile file, size_t offset, const void *src, size_t size) {
    const char *name;
    if (!valid_file(file, &name)) return -1;
    if (size && !src) {
        save_error_text("write buffer is null");
        return -1;
    }
    if (offset > PLAT_SAVE_MAX_BYTES || size > PLAT_SAVE_MAX_BYTES - offset) {
        save_error_text("save write exceeds the supported size");
        return -1;
    }

    size_t new_size = size;
    unsigned char *payload = NULL;
    if (offset == 0) {
        payload = (unsigned char *)malloc(size ? size : 1);
        if (!payload) {
            save_error_text("out of memory while writing save");
            return -1;
        }
        if (size) memcpy(payload, src, size);
    } else {
        struct stat st;
        int present = stat_file(file, &name, &st);
        if (present != 1) {
            if (present == 0) save_error_text("offset save write requires an existing file");
            return -1;
        }
        size_t old_size = (size_t)st.st_size;
        new_size = old_size > offset + size ? old_size : offset + size;
        payload = (unsigned char *)calloc(new_size ? new_size : 1, 1);
        if (!payload) {
            save_error_text("out of memory while updating save");
            return -1;
        }
        int fd = openat(g_directory_fd, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        if (fd < 0) {
            free(payload);
            save_error("cannot open save for update");
            return -1;
        }
        struct stat opened;
        int ok = fstat(fd, &opened) == 0 && S_ISREG(opened.st_mode) &&
                 opened.st_dev == st.st_dev && opened.st_ino == st.st_ino &&
                 read_fd_exact(fd, payload, old_size, 0);
        int saved_errno = errno;
        (void)close(fd);
        if (!ok) {
            free(payload);
            errno = saved_errno;
            save_error("cannot read save for update");
            return -1;
        }
        if (size) memcpy(payload + offset, src, size);
    }

    int ok = atomic_replace(name, payload, new_size);
    free(payload);
    return ok ? 0 : -1;
}

int plat_save_remove(PlatSaveFile file) {
    const char *name;
    struct stat st;
    int rc = stat_file(file, &name, &st);
    if (rc != 1) {
        if (rc == 0) save_error_text("save file does not exist");
        return -1;
    }
    if (unlinkat(g_directory_fd, name, 0) != 0) {
        save_error("cannot remove save file");
        return -1;
    }
    if (fsync(g_directory_fd) != 0) {
        save_error("cannot sync save directory after removal");
        return -1;
    }
    g_error[0] = '\0';
    return 0;
}
