/* CPU-side model of the PS1 GPU video memory (final-target code: plain C99, standard headers only, no PS1 types,
 * no guest addresses, no global state).
 *
 * The model is a 1024 x 512 array of 16-bit pixels: bits 0-4 red, 5-9 green, 10-14 blue, bit 15 the mask bit.
 * Every function takes the model by pointer. GpuVram is about 1 MiB, so allocate it on the heap or as a static
 * object rather than as a local in a small stack.
 *
 * Rules followed (psx-spx GPU chapter, and runtime/src/gpu.c for the reference behaviour):
 *   - CPU->VRAM load, VRAM->VRAM copy and VRAM->CPU store wrap at the 1024 and 512 edges, for both the origin and
 *     every row or column of the rectangle.
 *   - Fill ignores the drawing area and the mask bits. Its left edge is taken down to a multiple of 16 and its width
 *     is rounded up to a multiple of 16. Its colour is 15-bit with the mask bit cleared.
 *   - Load and copy honour the two mask flags (GPU_MASK_SET, GPU_MASK_CHECK), the same bits as GP0(E6).
 *
 * Dirty tracking: every write records the rectangle it touched. The shell uploads only those rectangles. The list
 * holds up to GPU_VRAM_DIRTY_MAX rectangles. Overlapping or touching rectangles are merged; when the list is full,
 * everything is collapsed into one bounding rectangle. The tracker may over-cover but never under-covers. */
#ifndef PLAT_GPU_VRAM_H
#define PLAT_GPU_VRAM_H

#include <stddef.h>
#include <stdint.h>

#define GPU_VRAM_W 1024
#define GPU_VRAM_H 512
#define GPU_VRAM_DIRTY_MAX 32

typedef struct {
    int x, y, w, h;   /* pixels; x in 0..1023, y in 0..511 when stored in the dirty list */
} GpuRect;

typedef struct {
    uint16_t px[GPU_VRAM_H][GPU_VRAM_W];    /* px[y][x] */
    GpuRect dirty[GPU_VRAM_DIRTY_MAX];
    int dirty_n;
} GpuVram;

#define GPU_MASK_SET   0x1u   /* written pixels get bit 15 forced to 1 */
#define GPU_MASK_CHECK 0x2u   /* destination pixels whose bit 15 is set are not overwritten */

void gpu_vram_init(GpuVram *v);   /* all pixels 0, dirty list empty */

/* 5-5-5 colour from 8-bit components (mask bit 0). */
uint16_t gpu_vram_color15(unsigned r, unsigned g, unsigned b);

/* Host -> VRAM. src holds w*h pixels, row by row, no padding. x and y may be any int and wrap. w must be 1..1024
 * and h 1..512; anything else is a no-op. */
void gpu_vram_load(GpuVram *v, int x, int y, int w, int h, const uint16_t *src, unsigned mask_flags);

/* Host -> VRAM from GP0(A0) payload words. Pixels form one continuous stream of halfwords: pixel k is the low half
 * of words[k/2] when k is even and the high half when k is odd. The stream runs across rows, so a row does not
 * start on a word boundary when w is odd. words must hold (w*h+1)/2 words. */
void gpu_vram_load_words(GpuVram *v, int x, int y, int w, int h, const uint32_t *words, unsigned mask_flags);

/* VRAM -> host. dst holds w*h pixels, raw (mask bit as stored). Same size rules as gpu_vram_load. */
void gpu_vram_store(const GpuVram *v, int x, int y, int w, int h, uint16_t *dst);

/* VRAM -> VRAM. Source and destination may overlap. Returns 0, or -1 when the temporary buffer could not be
 * allocated (nothing is written then). Same size rules as gpu_vram_load. */
int gpu_vram_copy(GpuVram *v, int sx, int sy, int dx, int dy, int w, int h, unsigned mask_flags);

/* Fill, GP0(02). x and y wrap. The left edge becomes x & ~15 and the width becomes (w + 15) & ~15, clamped to 1024.
 * h is used as given, clamped to 512. w or h <= 0 is a no-op. The colour is written with the mask bit cleared. */
void gpu_vram_fill(GpuVram *v, int x, int y, int w, int h, uint16_t color15);

/* Dirty tracking. gpu_vram_dirty_add records a rectangle by hand (for example after the shell writes pixels
 * directly); it is clamped to VRAM and does not wrap. */
void gpu_vram_dirty_add(GpuVram *v, int x, int y, int w, int h);
int  gpu_vram_dirty_count(const GpuVram *v);
int  gpu_vram_dirty_get(const GpuVram *v, int i, GpuRect *out);   /* 0 when i is out of range */
void gpu_vram_clear_dirty(GpuVram *v);

#endif
