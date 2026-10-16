/* Phase 1 native math/libc adapters. Native math APIs use raw IEEE bit
 * patterns; guest libc pointers are translated only at this boundary. */
#include "jojo_guest.h"
#include "jojo_core.h"
#include "plat_math.h"
#include "cpu_state.h"
#include "psx_memory.h"

#include <stddef.h>
#include <stdint.h>

extern uint8_t *memory_get_ram_ptr(void);
extern uint8_t *memory_get_scratchpad_ptr(void);

#define LLE_DECL(addr) extern void jojo_lle_func_##addr(CPUState *cpu)
LLE_DECL(80035C10); LLE_DECL(80035C50); LLE_DECL(80035C80);
LLE_DECL(8003D020); LLE_DECL(8003D1D0); LLE_DECL(8003D200); LLE_DECL(8003D3C0);
LLE_DECL(8003D470); LLE_DECL(8003D650); LLE_DECL(8003D720); LLE_DECL(8003D860);
LLE_DECL(8003DDC0);

static uint8_t *guest_span(uint32_t addr, uint32_t n) {
    uint32_t phys = addr & 0x1fffffffu;
    uint32_t off;
    if (phys >= 0x1f800000u && phys < 0x1f800400u &&
        n <= 0x1f800400u - phys)
        return memory_get_scratchpad_ptr() + (phys - 0x1f800000u);
    if (!psx_ram_resolve(addr, n, &off)) return NULL;
    return memory_get_ram_ptr() + off;
}

#define DECLARE_ARGS() \
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], \
             a2 = cpu->gpr[6], a3 = cpu->gpr[7]
#define NOTE(addr) jojo_note_call((addr), cpu->gpr[31])
#define TRACE(addr) jojo_trace_call((addr), a0, a1, a2, a3, cpu->gpr[2])
#define FALLBACK(addr, fn) do { \
    uint64_t start_ = jojo_cycles_now(); fn(cpu); \
    jojo_note_cycles((addr), jojo_cycles_now() - start_); TRACE(addr); return; \
} while (0)
#define DONE(addr) do { TRACE(addr); jojo_return(cpu); } while (0)

#ifndef JOJO_MATH_CYCLES
#define JOJO_MATH_CYCLES 0
#endif

void func_8003D020(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003D020);
    if (!jojo_piece_on(JOJO_PIECE_MATH)) FALLBACK(0x8003D020, jojo_lle_func_8003D020);
    cpu->gpr[2] = plat_sf_add_f32(a0, a1); jojo_cycles_charge(JOJO_MATH_CYCLES); DONE(0x8003D020);
}

void func_8003D1D0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003D1D0);
    if (!jojo_piece_on(JOJO_PIECE_MATH)) FALLBACK(0x8003D1D0, jojo_lle_func_8003D1D0);
    cpu->gpr[2] = plat_sf_ne_f32(a0, a1); jojo_cycles_charge(JOJO_MATH_CYCLES); DONE(0x8003D1D0);
}

void func_8003D200(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003D200);
    if (!jojo_piece_on(JOJO_PIECE_MATH)) FALLBACK(0x8003D200, jojo_lle_func_8003D200);
    cpu->gpr[2] = (uint32_t)plat_sf_f64_to_i32(a0, a1); jojo_cycles_charge(JOJO_MATH_CYCLES); DONE(0x8003D200);
}

void func_8003D3C0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003D3C0);
    if (!jojo_piece_on(JOJO_PIECE_MATH)) FALLBACK(0x8003D3C0, jojo_lle_func_8003D3C0);
    cpu->gpr[2] = (uint32_t)plat_sf_f32_to_i32(a0); jojo_cycles_charge(JOJO_MATH_CYCLES); DONE(0x8003D3C0);
}

void func_8003D470(CPUState *cpu) {
    DECLARE_ARGS(); uint32_t lo, hi; NOTE(0x8003D470);
    if (!jojo_piece_on(JOJO_PIECE_MATH)) FALLBACK(0x8003D470, jojo_lle_func_8003D470);
    plat_sf_i32_to_f64(a0, &lo, &hi); cpu->gpr[2] = lo; cpu->gpr[3] = hi;
    jojo_cycles_charge(JOJO_MATH_CYCLES); DONE(0x8003D470);
}

void func_8003D650(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003D650);
    if (!jojo_piece_on(JOJO_PIECE_MATH)) FALLBACK(0x8003D650, jojo_lle_func_8003D650);
    cpu->gpr[2] = plat_sf_i32_to_f32(a0); jojo_cycles_charge(JOJO_MATH_CYCLES); DONE(0x8003D650);
}

void func_8003D720(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003D720);
    if (!jojo_piece_on(JOJO_PIECE_MATH)) FALLBACK(0x8003D720, jojo_lle_func_8003D720);
    cpu->gpr[2] = plat_sf_mul_f32(a0, a1); jojo_cycles_charge(JOJO_MATH_CYCLES); DONE(0x8003D720);
}

void func_8003D860(CPUState *cpu) {
    DECLARE_ARGS(); uint32_t lo, hi; NOTE(0x8003D860);
    if (!jojo_piece_on(JOJO_PIECE_MATH)) FALLBACK(0x8003D860, jojo_lle_func_8003D860);
    plat_sf_sub_f64(a0, a1, a2, a3, &lo, &hi); cpu->gpr[2] = lo; cpu->gpr[3] = hi;
    jojo_cycles_charge(JOJO_MATH_CYCLES); DONE(0x8003D860);
}

/* 0x8003DC00 is not a function entry (data word / mid-function fragment, IDA pass finding 7): nothing is bound there. */

void func_8003DDC0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003DDC0);
    if (!jojo_piece_on(JOJO_PIECE_MATH)) FALLBACK(0x8003DDC0, jojo_lle_func_8003DDC0);
    cpu->gpr[2] = plat_sf_sub_f32(a0, a1); jojo_cycles_charge(JOJO_MATH_CYCLES); DONE(0x8003DDC0);
}

void func_80035C10(CPUState *cpu) {
    DECLARE_ARGS(); uint8_t *src, *dst; NOTE(0x80035C10);
    if (!jojo_piece_on(JOJO_PIECE_MATH)) FALLBACK(0x80035C10, jojo_lle_func_80035C10);
    src = a2 ? guest_span(a0, a2) : NULL; dst = a2 ? guest_span(a1, a2) : NULL;
    if (a2 && (!src || !dst)) FALLBACK(0x80035C10, jojo_lle_func_80035C10);
    if (a2) (void)plat_bcopy(src, dst, a2);
    cpu->gpr[2] = a0; jojo_cycles_charge(JOJO_MATH_CYCLES); DONE(0x80035C10);
}

void func_80035C50(CPUState *cpu) {
    DECLARE_ARGS(); uint8_t *dst; NOTE(0x80035C50);
    if (!jojo_piece_on(JOJO_PIECE_MATH)) FALLBACK(0x80035C50, jojo_lle_func_80035C50);
    dst = a1 ? guest_span(a0, a1) : NULL;
    if (a1 && !dst) FALLBACK(0x80035C50, jojo_lle_func_80035C50);
    if (a1) plat_bzero(dst, a1);
    cpu->gpr[2] = 0;   /* the original leaves v0 = 0 on every path (80035c50..80035c64) */
    jojo_cycles_charge(JOJO_MATH_CYCLES); DONE(0x80035C50);
}

void func_80035C80(CPUState *cpu) {
    DECLARE_ARGS(); uint8_t *dst; NOTE(0x80035C80);
    if (!jojo_piece_on(JOJO_PIECE_MATH)) FALLBACK(0x80035C80, jojo_lle_func_80035C80);
    dst = a2 ? guest_span(a0, a2) : NULL;
    if (a2 && !dst) FALLBACK(0x80035C80, jojo_lle_func_80035C80);
    if (a2) (void)plat_memset(dst, (int)a1, a2);
    cpu->gpr[2] = a0; jojo_cycles_charge(JOJO_MATH_CYCLES); DONE(0x80035C80);
}

