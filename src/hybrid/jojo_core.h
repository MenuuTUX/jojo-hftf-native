/* H1 hybrid glue (development only, never part of the final target): piece switches, per-caller-region call counts and the
 * per-frame state recorder shared by every Phase 1 adapter. See ../PHASE1_DESIGN.md sections 3 and 9.
 *
 * Switches (read once, on first use):
 *   JOJO_PHASE1=all            every piece on
 *   JOJO_PHASE1=pad,saves,...  the listed pieces on
 *   JOJO_<PIECE>=1             one piece on (JOJO_PAD, JOJO_SAVES, JOJO_GTE, JOJO_MATH, JOJO_CD, JOJO_GPU, JOJO_AUDIO, JOJO_KERNEL)
 *   (nothing set)              REFERENCE behavior: every wrapper calls the original generated body
 * JOJO_FILE_LAYER keeps its own switch (piece B3, see psx_cd_sector_override.c); JOJO_PHASE1=all also turns it on. */
#ifndef JOJO_CORE_H
#define JOJO_CORE_H
#include <stdint.h>
#include <stddef.h>

typedef enum {
    JOJO_PIECE_PAD, JOJO_PIECE_SAVES, JOJO_PIECE_GTE, JOJO_PIECE_MATH, JOJO_PIECE_CD, JOJO_PIECE_GPU,
    JOJO_PIECE_AUDIO, JOJO_PIECE_KERNEL, JOJO_PIECE_COUNT
} JojoPiece;

int  jojo_piece_on(JojoPiece p);              /* 1 when the piece is switched on */
const char *jojo_piece_name(JojoPiece p);

/* Call counting: every wrapper calls jojo_note_call(its guest address, $ra) first, whether the piece is on or off. */
void jojo_note_call(uint32_t addr, uint32_t ra);

/* Call/return trace (JOJO_TRACE=<path>): one text line per traced call: frame, function, a0..a3, v0 after the call. Wrappers that delegate to the
 * original body call this after it returns; with the piece on they call it after their own result is set, so the two traces diff directly. */
void jojo_trace_call(uint32_t addr, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t v0);
uint64_t jojo_frame_now(void);

/* Guest-cycle accounting. The original library bodies charge guest cycles as they run; a native replacement that charges none shifts every later
 * device event (CD sectors, DMA) against the frame boundaries. Wrappers measure the original's cost with jojo_cycles_now() when the piece is off
 * (reported by the "counts" command as cycles per call) and charge the measured amount with jojo_cycles_charge() when it is on. */
uint64_t jojo_cycles_now(void);
void     jojo_cycles_charge(uint32_t cycles);
void     jojo_note_cycles(uint32_t addr, uint64_t cycles);
/* Per-frame cycle charge for the library work that ran inside the vblank interrupt in the original (validation aid, see PHASE1_DESIGN.md section 9):
 * JOJO_CYCLE_TABLE=<file of u32, indexed by the frame number at the frame hook> gives an exact value per frame, JOJO_FRAMECYC=<n> a constant,
 * otherwise `dflt` is returned. */
uint32_t jojo_frame_cycles(uint64_t frame, uint32_t dflt);

/* Device-access counters (step 7): the runtime hooks call these; the report lists them per device. */
typedef enum { JOJO_DEV_GPU, JOJO_DEV_SPU, JOJO_DEV_CD, JOJO_DEV_SIO, JOJO_DEV_MEMCARD, JOJO_DEV_COUNT } JojoDevice;
void jojo_note_device(JojoDevice d);
uint64_t jojo_device_count(JojoDevice d);   /* running total, for the differential harness (jojo_diff.c) */

/* Frame hook from the debug server patch (debug_frame_hook.patch). */
void jojo_frame_hook(uint64_t frame);
int  jojo_debug_command(const char *json, char *out, size_t out_size);

/* Small helpers for the JSON of the "jojo" command (the framework parser is private to debug_server.c). */
int  jojo_json_int(const char *json, const char *key, int def);
int  jojo_json_str(const char *json, const char *key, char *dst, size_t n);

#endif
