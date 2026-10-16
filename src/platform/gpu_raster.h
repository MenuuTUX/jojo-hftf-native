/* Thread-safe handoff between the H1 GPU adapter and the Metal presenter.
 * The Metal side receives bounded copies; it never observes mutable VRAM.
 *
 * ORDERED STREAM
 * ==============
 * Guest drawing is recorded as one ordered stream per frame. Each entry is a GpItem, in the order the guest issued it:
 *   GP_ITEM_FILL, GP_ITEM_COPY_VRAM, GP_ITEM_COPY_TO_VRAM   VRAM operations. The producer applies each one to the CPU
 *                                                            VRAM model at the moment it is recorded.
 *   GP_ITEM_TRI, GP_ITEM_QUAD, GP_ITEM_LINE, GP_ITEM_RECT     draws. They are not rasterised into VRAM; they sample it.
 * GP_ITEM_COPY_TO_VRAM pixels live in the stream's payload array; the entry's payload_off indexes that array. Entries of
 * type GP_ITEM_ENV and GP_ITEM_COPY_TO_CPU never enter the stream: every draw item already carries the environment
 * state that was active when it was decoded.
 *
 * Producer (guest thread): gpu_raster_record() takes decoded GP0 output. gpu_raster_load_words(), gpu_raster_fill() and
 * gpu_raster_copy() record the same entries for the direct library calls (LoadImage, ClearImage, MoveImage). Calls must
 * be made in guest order. gpu_raster_publish_frame() closes the frame; the stream then becomes the published stream.
 *
 * Replay (any thread, on a published copy): gpu_raster_replay() runs a stream against a caller-owned VRAM model. For
 * each draw it calls the visitor with the VRAM as it stands at that position of the stream, so a draw that samples a
 * texture sees the texels that were there when the draw was issued, not the end-of-frame image.
 *
 * Compatibility: gpu_raster_append(), gpu_raster_copy_published() and the end-of-frame VRAM snapshot keep their old
 * meaning. The Metal shell ignores VRAM entries in the item list (it renders draws from end-of-frame VRAM), so it is
 * unaffected by them, but the stream counts against GPU_RASTER_MAX_ITEMS. */
#ifndef PLAT_GPU_RASTER_H
#define PLAT_GPU_RASTER_H

#include "gpu_gp0.h"
#include "gpu_soft.h"
#include "gpu_vram.h"

#include <stddef.h>
#include <stdint.h>

/* Entries per frame, draws and VRAM operations together. Equal to the Metal shell's item capacity, so it must not grow
 * without changing GpuMetal.swift. */
#define GPU_RASTER_MAX_ITEMS 8192u
/* CPU->VRAM pixel words per frame (1 MiB). */
#define GPU_RASTER_MAX_PAYLOAD_WORDS 262144u
#define GPU_RASTER_VRAM_PIXELS ((size_t)GPU_VRAM_W * (size_t)GPU_VRAM_H)

typedef struct {
    uint16_t x, y, w, h;
    uint8_t enabled;
    uint8_t is_rgb24;
    uint8_t interlace;
    uint8_t reverse;
} GpuDisplayEnv;

/* Return codes of the functions below: 0 on success, -1 when the entry was refused (stream or payload full, or an
 * invalid entry; nothing was applied), -2 when a replay meets an entry type that cannot appear in a stream. */

void gpu_raster_reset(void);
void gpu_raster_set_display(const GpuDisplayEnv *display);
/* Rasterisation switch (default on). When off, draw entries are recorded but do not change VRAM (debugging only). */
void gpu_raster_set_soft(int on);
/* Provenance (debugging): when on, every pixel written during the frame records the stream entry that wrote it (index + 1,
 * 0 = not written in this frame). Reset at each publish. gpu_raster_copy_prov copies the live array; gpu_raster_copy_stream
 * copies the entries of the frame that is still open (the ones the provenance refers to). */
void gpu_raster_set_prov(int on);
int    gpu_raster_copy_prov(uint32_t *prov, size_t capacity);
size_t gpu_raster_copy_stream(GpItem *items, size_t capacity);

/* Producer. gpu_raster_record applies and records a batch in order. Each VRAM entry needs its pixels in payload (for
 * COPY_TO_VRAM); a batch that does not fit is refused as a whole. ENV and COPY_TO_CPU entries are skipped. */
int gpu_raster_record(const GpItem *items, size_t count, const uint32_t *payload, size_t payload_count);
/* gpu_raster_append(items, count) is gpu_raster_record(items, count, NULL, 0): draws and VRAM fill/copy entries only. */
int gpu_raster_append(const GpItem *items, size_t count);

/* Direct library calls. Each applies the operation to the CPU VRAM model and records it. If the stream is full the VRAM
 * is still updated, the entry is dropped and the frame is marked incomplete (see gpu_raster_copy_published_ordered).
 * gpu_raster_fill stores its colour with the mask bit cleared; the rounding of the fill rectangle is done by gpu_vram. */
void gpu_raster_load_words(int x, int y, int w, int h, const uint32_t *words, unsigned mask_flags);
void gpu_raster_store(int x, int y, int w, int h, uint16_t *pixels);
int  gpu_raster_copy(int sx, int sy, int dx, int dy, int w, int h, unsigned mask_flags);
void gpu_raster_fill(int x, int y, int w, int h, uint16_t color15);

/* Closes the frame: the stream and the VRAM model are copied into the published buffers. */
void gpu_raster_publish_frame(void);

/* Consumer for the Metal shell (unchanged signature). Copies the published draws and the end-of-frame VRAM. VRAM
 * entries are included in the item list; the shell ignores them. */
int gpu_raster_copy_published(GpItem *items, size_t item_capacity, size_t *item_count,
                              uint16_t *vram, size_t vram_capacity, GpuDisplayEnv *display,
                              uint64_t *sequence);

/* Consumer for the ordered replay. Copies the published stream with its payload and the end-of-frame VRAM under one
 * lock. Returns 1 when everything was copied, 0 when a buffer is too small (nothing copied), -1 when the frame is
 * incomplete (some entries were dropped; everything else was still copied). item_count, payload_count and sequence
 * may be NULL. */
int gpu_raster_copy_published_ordered(GpItem *items, size_t item_capacity, size_t *item_count,
                                      uint32_t *payload, size_t payload_capacity, size_t *payload_count,
                                      uint16_t *vram, size_t vram_capacity,
                                      GpuDisplayEnv *display, uint64_t *sequence);

/* Copies the live CPU VRAM model (not the published copy) under the lock. For diagnostics. Returns 1 on success. */
int gpu_raster_copy_live_vram(uint16_t *vram, size_t vram_capacity);

/* Ordered replay. Applies count entries of a stream to vram in order, exactly as the producer did: VRAM entries are
 * applied, and each draw is rasterised with gpu_soft (cfg) into vram. For each draw, visit (may be NULL) is called
 * BEFORE the draw is applied, with vram as it stands at that position of the stream. The caller initialises vram
 * (gpu_vram_init) and passes the stream's payload with payload_count words. Returns 0, -1 for an entry whose pixels are
 * missing or out of range, or -2 for an entry type that cannot appear in a stream. The visitor must not change vram. */
typedef void (*GpuReplayVisitFn)(void *ctx, const GpItem *draw, const GpuVram *vram_at_draw);
int gpu_raster_replay(GpuVram *vram, const GpuSoftConfig *cfg, const GpItem *items, size_t count,
                      const uint32_t *payload, size_t payload_count,
                      GpuReplayVisitFn visit, void *ctx);

#endif
