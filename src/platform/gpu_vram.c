/* CPU-side PS1 VRAM model. See gpu_vram.h for the rules. Plain C99, standard headers only. */
#include "gpu_vram.h"

#include <stdlib.h>
#include <string.h>

static int wrap(int v, int n) {
    v %= n;
    return v < 0 ? v + n : v;
}

static int rect_touch(GpuRect a, GpuRect b) {
    return a.x <= b.x + b.w && b.x <= a.x + a.w && a.y <= b.y + b.h && b.y <= a.y + a.h;
}

static GpuRect rect_union(GpuRect a, GpuRect b) {
    int x0 = a.x < b.x ? a.x : b.x;
    int y0 = a.y < b.y ? a.y : b.y;
    int x1 = (a.x + a.w > b.x + b.w) ? a.x + a.w : b.x + b.w;
    int y1 = (a.y + a.h > b.y + b.h) ? a.y + a.h : b.y + b.h;
    GpuRect r;
    r.x = x0;
    r.y = y0;
    r.w = x1 - x0;
    r.h = y1 - y0;
    return r;
}

/* Adds one non-wrapping rectangle that lies inside VRAM. */
static void dirty_add_plain(GpuVram *v, GpuRect cur) {
    int i = 0;
    while (i < v->dirty_n) {
        if (rect_touch(v->dirty[i], cur)) {
            cur = rect_union(cur, v->dirty[i]);
            v->dirty[i] = v->dirty[--v->dirty_n];
            i = 0;   /* cur grew, so earlier entries may now touch it */
        } else {
            ++i;
        }
    }
    if (v->dirty_n < GPU_VRAM_DIRTY_MAX) {
        v->dirty[v->dirty_n++] = cur;
        return;
    }
    /* List full: collapse everything into one bounding rectangle. Over-covers, never under-covers. */
    {
        GpuRect all = cur;
        for (i = 0; i < v->dirty_n; ++i) {
            all = rect_union(all, v->dirty[i]);
        }
        v->dirty[0] = all;
        v->dirty_n = 1;
    }
}

/* Marks a wrapped rectangle (origin may be any int, w <= 1024, h <= 512) as at most four plain rectangles. */
static void dirty_mark_wrapped(GpuVram *v, int x, int y, int w, int h) {
    int x0 = wrap(x, GPU_VRAM_W);
    int y0 = wrap(y, GPU_VRAM_H);
    int w1 = (x0 + w <= GPU_VRAM_W) ? w : GPU_VRAM_W - x0;
    int w2 = w - w1;
    int h1 = (y0 + h <= GPU_VRAM_H) ? h : GPU_VRAM_H - y0;
    int h2 = h - h1;
    GpuRect r;

    r.x = x0; r.y = y0; r.w = w1; r.h = h1;
    dirty_add_plain(v, r);
    if (w2 > 0) {
        r.x = 0; r.y = y0; r.w = w2; r.h = h1;
        dirty_add_plain(v, r);
    }
    if (h2 > 0) {
        r.x = x0; r.y = 0; r.w = w1; r.h = h2;
        dirty_add_plain(v, r);
        if (w2 > 0) {
            r.x = 0; r.y = 0; r.w = w2; r.h = h2;
            dirty_add_plain(v, r);
        }
    }
}

static void put_pixel(GpuVram *v, int x, int y, uint16_t pix, unsigned mask_flags) {
    uint16_t *dst = &v->px[y][x];
    if ((mask_flags & GPU_MASK_CHECK) && (*dst & 0x8000u)) {
        return;
    }
    if (mask_flags & GPU_MASK_SET) {
        pix |= 0x8000u;
    }
    *dst = pix;
}

static int size_ok(int w, int h) {
    return w > 0 && h > 0 && w <= GPU_VRAM_W && h <= GPU_VRAM_H;
}

void gpu_vram_init(GpuVram *v) {
    memset(v, 0, sizeof *v);
}

uint16_t gpu_vram_color15(unsigned r, unsigned g, unsigned b) {
    return (uint16_t)(((r >> 3) & 31u) | (((g >> 3) & 31u) << 5) | (((b >> 3) & 31u) << 10));
}

void gpu_vram_load(GpuVram *v, int x, int y, int w, int h, const uint16_t *src, unsigned mask_flags) {
    int i, j;
    if (!v || !src || !size_ok(w, h)) {
        return;
    }
    for (j = 0; j < h; ++j) {
        int py = wrap(y + j, GPU_VRAM_H);
        for (i = 0; i < w; ++i) {
            put_pixel(v, wrap(x + i, GPU_VRAM_W), py, src[(size_t)j * (size_t)w + (size_t)i], mask_flags);
        }
    }
    dirty_mark_wrapped(v, x, y, w, h);
}

void gpu_vram_load_words(GpuVram *v, int x, int y, int w, int h, const uint32_t *words, unsigned mask_flags) {
    size_t k, total;
    if (!v || !words || !size_ok(w, h)) {
        return;
    }
    total = (size_t)w * (size_t)h;
    for (k = 0; k < total; ++k) {
        uint32_t word = words[k >> 1];
        uint16_t pix = (uint16_t)((k & 1u) ? (word >> 16) : (word & 0xFFFFu));
        int i = (int)(k % (size_t)w);
        int j = (int)(k / (size_t)w);
        put_pixel(v, wrap(x + i, GPU_VRAM_W), wrap(y + j, GPU_VRAM_H), pix, mask_flags);
    }
    dirty_mark_wrapped(v, x, y, w, h);
}

void gpu_vram_store(const GpuVram *v, int x, int y, int w, int h, uint16_t *dst) {
    int i, j;
    if (!v || !dst || !size_ok(w, h)) {
        return;
    }
    for (j = 0; j < h; ++j) {
        int py = wrap(y + j, GPU_VRAM_H);
        for (i = 0; i < w; ++i) {
            dst[(size_t)j * (size_t)w + (size_t)i] = v->px[py][wrap(x + i, GPU_VRAM_W)];
        }
    }
}

int gpu_vram_copy(GpuVram *v, int sx, int sy, int dx, int dy, int w, int h, unsigned mask_flags) {
    uint16_t *tmp;
    size_t n;
    if (!v || !size_ok(w, h)) {
        return 0;
    }
    n = (size_t)w * (size_t)h;
    tmp = (uint16_t *)malloc(n * sizeof *tmp);
    if (!tmp) {
        return -1;
    }
    /* Read the whole source before writing, so overlapping source and destination are handled. */
    gpu_vram_store(v, sx, sy, w, h, tmp);
    gpu_vram_load(v, dx, dy, w, h, tmp, mask_flags);
    free(tmp);
    return 0;
}

void gpu_vram_fill(GpuVram *v, int x, int y, int w, int h, uint16_t color15) {
    int x0, wa, i, j;
    uint16_t colour;
    if (!v || w <= 0 || h <= 0) {
        return;
    }
    if (w > GPU_VRAM_W) w = GPU_VRAM_W;
    if (h > GPU_VRAM_H) h = GPU_VRAM_H;
    x0 = wrap(x, GPU_VRAM_W) & ~15;
    wa = (w + 15) & ~15;
    if (wa > GPU_VRAM_W) wa = GPU_VRAM_W;
    colour = (uint16_t)(color15 & 0x7FFFu);
    for (j = 0; j < h; ++j) {
        int py = wrap(y + j, GPU_VRAM_H);
        for (i = 0; i < wa; ++i) {
            v->px[py][wrap(x0 + i, GPU_VRAM_W)] = colour;
        }
    }
    dirty_mark_wrapped(v, x0, y, wa, h);
}

void gpu_vram_dirty_add(GpuVram *v, int x, int y, int w, int h) {
    GpuRect r;
    if (!v) {
        return;
    }
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > GPU_VRAM_W) w = GPU_VRAM_W - x;
    if (y + h > GPU_VRAM_H) h = GPU_VRAM_H - y;
    if (w <= 0 || h <= 0) {
        return;
    }
    r.x = x; r.y = y; r.w = w; r.h = h;
    dirty_add_plain(v, r);
}

int gpu_vram_dirty_count(const GpuVram *v) {
    return v ? v->dirty_n : 0;
}

int gpu_vram_dirty_get(const GpuVram *v, int i, GpuRect *out) {
    if (!v || !out || i < 0 || i >= v->dirty_n) {
        return 0;
    }
    *out = v->dirty[i];
    return 1;
}

void gpu_vram_clear_dirty(GpuVram *v) {
    if (v) {
        v->dirty_n = 0;
    }
}
