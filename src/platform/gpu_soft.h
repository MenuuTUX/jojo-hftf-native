/* CPU software rasterizer for decoded GP0 draw items (final-target code: plain C99, standard headers only, no PS1 types,
 * no guest addresses, no global state).
 *
 * This is a port of the software renderer of the PSXRecomp runtime (runtime/src/gpu_sw_renderer.c, native scale-1 path)
 * together with the command-level rules of its GP0 front end (runtime/src/gpu.c: gp0_exec_*). The port reproduces the
 * reference's VRAM bit for bit for the default configuration: no bilinear filtering, no perspective correction, no
 * precise-triangle overrides, no supersampling, no native-wide compositor.
 *
 * Input is one GpItem of the ordered stream (see gpu_gp0.h and gpu_raster.h). Every field the reference reads at draw
 * time is carried by the item: the drawing area (E3/E4), the offset (E5, already added to the coordinates), the texture
 * window (E2), the mask flags (E6), the semi-transparency mode and the texpage word (E1 or the latch of a textured
 * polygon), the CLUT and texture coordinates, and the colours. VRAM operations (fill, copy, upload) are not handled here;
 * they belong to gpu_vram.
 *
 * Rules that are deliberately NOT applied, because the reference does not apply them either:
 *   - No dithering. E1 bit 9 is recorded (GP_F_DITHER) but never changes a pixel.
 *   - No mask-bit handling beyond E6 (set/check on each written pixel, as in the reference's put_opaque/put_textured).
 *   - Textured rectangles and sprites sample texels with the texpage state, not with a per-rectangle flip (the reference
 *     has no rectangle flip in this path). */
#ifndef PLAT_GPU_SOFT_H
#define PLAT_GPU_SOFT_H

#include "gpu_gp0.h"
#include "gpu_vram.h"

#include <stdint.h>

typedef struct {
    /* Display size used by the reference's full-screen quad rule (a monochrome quad covering [0,disp_w) x [0,disp_h) is drawn
     * as one rectangle). The reference reads it from the live display state; 0 means "use 320 x 240". */
    int disp_w;
    int disp_h;
    /* Optional provenance (debugging): when prov is not NULL, every pixel written by a draw gets prov_id stored at its VRAM
     * index (prov points to GPU_VRAM_W * GPU_VRAM_H entries). */
    uint32_t *prov;
    uint32_t prov_id;
} GpuSoftConfig;

/* Draws one item into vram. Items of other types are ignored. The item's own state (area, mask, semi, texpage, window)
 * is used, never the caller's. */
void gpu_soft_draw(GpuVram *vram, const GpItem *it, const GpuSoftConfig *cfg);

#endif
