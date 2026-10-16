/* Phase 1 libgte adapters: the libgte entries of PHASE1_SURFACE.md (section libgte) except InitGeom. Development-only hybrid glue,
 * same pattern as jojo_math.c. Each wrapper keeps the original's contract: $v0, the callee-saved registers, every guest word the
 * original writes (only through the cpu read and write callbacks), and the COP2 state. $v1 and the caller-saved temporaries are not part of the contract.
 *
 * COP2 state: cpu->gte_data / cpu->gte_ctrl are the arrays the LLE COP2 path uses, so wrapped and unwrapped functions see one GTE.
 * A wrapper copies them into a plat_gte_state for one call (gte_pull) and copies them back before it returns (gte_push).
 *
 * Not wrapped: 0x80039C08 InitGeom. It calls ExitCard (0x8003C120), which returns into the middle of InitGeom (0x80039C14) through a
 * saved return pointer, so a C wrapper cannot reproduce that continuation. The LLE body keeps running on the same COP2 arrays.
 *
 * 0x8003BFA0 ratan2 falls back to the original body on the two input shapes where the original executes break (divide by zero, or
 * -1 divided into 0x80000000). The original's trap path is kept by running it. */
#include "jojo_guest.h"
#include "jojo_core.h"
#include "cpu_state.h"
#include "plat_gte.h"

#include <stdint.h>
#include <string.h>

#define LLE_DECL(addr) extern void jojo_lle_func_##addr(CPUState *cpu)
LLE_DECL(80039A90); LLE_DECL(80039B60); LLE_DECL(80039C90); LLE_DECL(80039DC0);
LLE_DECL(80039FD0); LLE_DECL(8003A070); LLE_DECL(8003A1B0); LLE_DECL(8003A2F0);
LLE_DECL(8003A380); LLE_DECL(8003A3C0); LLE_DECL(8003A3E0); LLE_DECL(8003A714);
LLE_DECL(8003A7E0); LLE_DECL(8003A810); LLE_DECL(8003A840); LLE_DECL(8003A970);
LLE_DECL(8003AC00); LLE_DECL(8003BFA0);

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

#ifndef JOJO_GTE_CYCLES
#define JOJO_GTE_CYCLES 0
#endif

/* ---- guest access and arithmetic (MIPS semantics on 32-bit words) ---- */

static uint32_t rd_w(CPUState *cpu, uint32_t a) { return cpu->read_word(a); }
static uint32_t rd_h(CPUState *cpu, uint32_t a) { return (uint32_t)(int32_t)(int16_t)cpu->read_half(a); }
static void wr_w(CPUState *cpu, uint32_t a, uint32_t v) { cpu->write_word(a, v); }
static void wr_h(CPUState *cpu, uint32_t a, uint32_t v) { cpu->write_half(a, (uint16_t)v); }

static uint32_t sext16(uint32_t v) { return (uint32_t)(int32_t)(int16_t)(uint16_t)v; }

/* sra: arithmetic shift right of a 32-bit word (MIPS sra/srav, shift amount taken mod 32). */
static uint32_t sar32(uint32_t v, uint32_t n) {
    n &= 31u;
    if (n == 0u) return v;
    return (v >> n) | ((v & 0x80000000u) ? (0xFFFFFFFFu << (32u - n)) : 0u);
}

/* Low 32 bits of a product: the same bits as MIPS mult/multu mflo. */
static uint32_t mul32(uint32_t a, uint32_t b) { return a * b; }

/* COP2 state: copy the CPU arrays in and out around one call. */
static void gte_pull(const CPUState *cpu, plat_gte_state *s) {
    memcpy(s->data, cpu->gte_data, sizeof s->data);
    memcpy(s->ctrl, cpu->gte_ctrl, sizeof s->ctrl);
}

static void gte_push(CPUState *cpu, const plat_gte_state *s) {
    memcpy(cpu->gte_data, s->data, sizeof cpu->gte_data);
    memcpy(cpu->gte_ctrl, s->ctrl, sizeof cpu->gte_ctrl);
}

/* ---- rsin / rcos: quarter-wave halfword sine table at 0x8005C2E0, 4096 units per turn ---- */

/* Body of 0x80039ACC, angle already masked to 12 bits. */
static uint32_t gte_sin12(CPUState *cpu, uint32_t a) {
    if (a < 1025u) return rd_h(cpu, 0x8005C2E0u + 2u * a);
    if (a < 2049u) return rd_h(cpu, 0x8005C2E0u + 2u * (2048u - a));
    if (a < 3073u) return 0u - rd_h(cpu, 0x8005B2E0u + 2u * a);
    return 0u - rd_h(cpu, 0x8005C2E0u + 2u * (4096u - a));
}

/* Body of 0x80039B60. */
static uint32_t gte_cos12(CPUState *cpu, uint32_t a) {
    if (a & 0x80000000u) a = 0u - a;
    a &= 0xFFFu;
    if (a >= 2049u) {
        if (a < 3073u) return 0u - rd_h(cpu, 0x8005C2E0u + 2u * (3072u - a));
        return rd_h(cpu, 0x8005AAE0u + 2u * a);
    }
    if (a >= 1025u) return 0u - rd_h(cpu, 0x8005BAE0u + 2u * a);
    return rd_h(cpu, 0x8005C2E0u + 2u * (1024u - a));
}

void func_80039A90(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80039A90);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x80039A90, jojo_lle_func_80039A90);
    cpu->gpr[2] = (a0 & 0x80000000u) ? 0u - gte_sin12(cpu, (0u - a0) & 0xFFFu) : gte_sin12(cpu, a0 & 0xFFFu);
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x80039A90);
}

void func_80039B60(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80039B60);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x80039B60, jojo_lle_func_80039B60);
    cpu->gpr[2] = gte_cos12(cpu, a0);
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x80039B60);
}

/* ---- SquareRoot0 / SquareRoot12: LZCS/LZCR normalisation, then the 0x8005CB00 halfword table ---- */

/* Shared lookup of 0x80039C90 and 0x80039FD0: x shifted into table range by t2 = LZCR & ~1. Returns the sign-extended entry. */
static uint32_t gte_sqrt_tab(CPUState *cpu, uint32_t x, uint32_t t2) {
    int32_t t3 = (int32_t)t2 - 24;
    uint32_t t4 = (t3 < 0) ? sar32(x, (uint32_t)(24 - (int32_t)t2)) : (x << ((uint32_t)t3 & 31u));
    return rd_h(cpu, 0x8005CB00u + ((t4 - 64u) << 1));
}

void func_80039C90(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80039C90);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x80039C90, jojo_lle_func_80039C90);
    {
        plat_gte_state s; uint32_t lz, t2, t1, v0 = 0u;
        gte_pull(cpu, &s);
        plat_gte_write_data(&s, 30, a0);              /* mtc2 $a0, LZCS */
        lz = plat_gte_read_data(&s, 31);              /* mfc2 LZCR */
        gte_push(cpu, &s);
        if (lz != 32u) {
            t2 = lz & ~1u;
            t1 = sar32(31u - t2, 1u);
            v0 = (gte_sqrt_tab(cpu, a0, t2) << (t1 & 31u)) >> 12;
        }
        cpu->gpr[2] = v0;
    }
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x80039C90);
}

void func_80039FD0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80039FD0);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x80039FD0, jojo_lle_func_80039FD0);
    {
        plat_gte_state s; uint32_t lz, t2, t1, tb, v0 = 0u;
        gte_pull(cpu, &s);
        plat_gte_write_data(&s, 30, a0);              /* mtc2 $a0, LZCS */
        lz = plat_gte_read_data(&s, 31);              /* mfc2 LZCR */
        gte_push(cpu, &s);
        if (lz != 32u) {
            t2 = lz & ~1u;
            t1 = sar32(19u - t2, 1u);
            tb = gte_sqrt_tab(cpu, a0, t2);
            v0 = ((int32_t)t1 < 0) ? (tb >> ((0u - t1) & 31u)) : (tb << (t1 & 31u));
        }
        cpu->gpr[2] = v0;
    }
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x80039FD0);
}

/* ---- VectorNormal: 0x80039DC0 with its helper 0x80039E20 ---- */

/* Helper body: squared-sum normalisation of x[0..2] (SQR, LZCS/LZCR, table 0x8005CC94 as IR0, GPF). Writes out[0..2], returns the
 * squared sum (left in $v0 by the original), and reports LZCR & ~1 (left in $v1). */
static uint32_t gte_norm3(CPUState *cpu, plat_gte_state *s, const uint32_t x[3], uint32_t out[3], uint32_t *lz_out) {
    uint32_t sum, lz, sh, t4, t5;
    int32_t t3;
    plat_gte_write_data(s, 9, x[0]); plat_gte_write_data(s, 10, x[1]); plat_gte_write_data(s, 11, x[2]);
    plat_gte_command(s, 0x0A00428u);                  /* SQR sf=0 lm=1 */
    sum = plat_gte_read_data(s, 25) + plat_gte_read_data(s, 26) + plat_gte_read_data(s, 27);
    plat_gte_write_data(s, 30, sum);                  /* LZCS */
    lz = plat_gte_read_data(s, 31) & ~1u;             /* LZCR & ~1 */
    sh = sar32(31u - lz, 1u);
    t3 = (int32_t)lz - 24;
    t4 = (t3 < 0) ? sar32(sum, (uint32_t)(24 - (int32_t)lz)) : (sum << ((uint32_t)t3 & 31u));
    t5 = rd_h(cpu, 0x8005CC94u + ((t4 - 64u) << 1));
    plat_gte_write_data(s, 8, t5);                    /* IR0 */
    plat_gte_write_data(s, 9, x[0]); plat_gte_write_data(s, 10, x[1]); plat_gte_write_data(s, 11, x[2]);
    plat_gte_command(s, 0x190003Du);                  /* GPF sf=0 lm=0 */
    out[0] = sar32(plat_gte_read_data(s, 25), sh);
    out[1] = sar32(plat_gte_read_data(s, 26), sh);
    out[2] = sar32(plat_gte_read_data(s, 27), sh);
    *lz_out = lz;
    return sum;
}

void func_80039DC0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80039DC0);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x80039DC0, jojo_lle_func_80039DC0);
    {
        plat_gte_state s; uint32_t x[3], o[3], lz, v0;
        x[0] = rd_w(cpu, a0); x[1] = rd_w(cpu, a0 + 4u); x[2] = rd_w(cpu, a0 + 8u);
        gte_pull(cpu, &s);
        v0 = gte_norm3(cpu, &s, x, o, &lz);
        gte_push(cpu, &s);
        wr_w(cpu, a1, o[0]); wr_w(cpu, a1 + 4u, o[1]); wr_w(cpu, a1 + 8u, o[2]);
        cpu->gpr[2] = v0;
    }
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x80039DC0);
}

/* ---- ScaleMatrixL / ScaleMatrix: packed halfword pairs of MATRIX words times vector components ---- */

/* One packed word: low and high halfwords each multiplied, >> 12, low halfword kept, high halfword shifted up. */
static uint32_t gte_mat_pack(uint32_t w, uint32_t mlo, uint32_t mhi) {
    return (sar32(mul32(sext16(w), mlo), 12u) & 0xFFFFu) | (sar32(mul32(sar32(w, 16u), mhi), 12u) << 16);
}

void func_8003A070(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003A070);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x8003A070, jojo_lle_func_8003A070);
    {
        uint32_t vx = rd_w(cpu, a1), vy = rd_w(cpu, a1 + 4u), vz = rd_w(cpu, a1 + 8u);
        wr_w(cpu, a0 + 0x00u, gte_mat_pack(rd_w(cpu, a0 + 0x00u), vx, vx));
        wr_w(cpu, a0 + 0x04u, gte_mat_pack(rd_w(cpu, a0 + 0x04u), vx, vy));
        wr_w(cpu, a0 + 0x08u, gte_mat_pack(rd_w(cpu, a0 + 0x08u), vy, vy));
        wr_w(cpu, a0 + 0x0Cu, gte_mat_pack(rd_w(cpu, a0 + 0x0Cu), vz, vz));
        wr_w(cpu, a0 + 0x10u, sar32(mul32(sext16(rd_w(cpu, a0 + 0x10u)), vz), 12u));
        cpu->gpr[2] = a0;
    }
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x8003A070);
}

void func_8003A1B0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003A1B0);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x8003A1B0, jojo_lle_func_8003A1B0);
    {
        uint32_t vx = rd_w(cpu, a1), vy = rd_w(cpu, a1 + 4u), vz = rd_w(cpu, a1 + 8u);
        wr_w(cpu, a0 + 0x00u, gte_mat_pack(rd_w(cpu, a0 + 0x00u), vx, vy));
        wr_w(cpu, a0 + 0x04u, gte_mat_pack(rd_w(cpu, a0 + 0x04u), vz, vx));
        wr_w(cpu, a0 + 0x08u, gte_mat_pack(rd_w(cpu, a0 + 0x08u), vy, vz));
        wr_w(cpu, a0 + 0x0Cu, gte_mat_pack(rd_w(cpu, a0 + 0x0Cu), vx, vy));
        wr_w(cpu, a0 + 0x10u, sar32(mul32(sext16(rd_w(cpu, a0 + 0x10u)), vz), 12u));
        cpu->gpr[2] = a0;
    }
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x8003A1B0);
}

/* ---- COP2 control-register loads: SetRotMatrix, SetTransMatrix, SetGeomOffset, SetGeomScreen ---- */

void func_8003A2F0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003A2F0);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x8003A2F0, jojo_lle_func_8003A2F0);
    {
        plat_gte_state s;
        gte_pull(cpu, &s);
        plat_gte_write_ctrl(&s, 0, rd_w(cpu, a0));
        plat_gte_write_ctrl(&s, 1, rd_w(cpu, a0 + 4u));
        plat_gte_write_ctrl(&s, 2, rd_w(cpu, a0 + 8u));
        plat_gte_write_ctrl(&s, 3, rd_w(cpu, a0 + 12u));
        plat_gte_write_ctrl(&s, 4, rd_w(cpu, a0 + 16u));
        gte_push(cpu, &s);
    }
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x8003A2F0);
}

void func_8003A380(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003A380);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x8003A380, jojo_lle_func_8003A380);
    {
        plat_gte_state s;
        gte_pull(cpu, &s);
        plat_gte_write_ctrl(&s, 5, rd_w(cpu, a0 + 0x14u));
        plat_gte_write_ctrl(&s, 6, rd_w(cpu, a0 + 0x18u));
        plat_gte_write_ctrl(&s, 7, rd_w(cpu, a0 + 0x1Cu));
        gte_push(cpu, &s);
    }
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x8003A380);
}

void func_8003A3C0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003A3C0);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x8003A3C0, jojo_lle_func_8003A3C0);
    {
        plat_gte_state s;
        gte_pull(cpu, &s);
        plat_gte_write_ctrl(&s, 24, a0 << 16);
        plat_gte_write_ctrl(&s, 25, a1 << 16);
        gte_push(cpu, &s);
    }
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x8003A3C0);
}

void func_8003A3E0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003A3E0);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x8003A3E0, jojo_lle_func_8003A3E0);
    {
        plat_gte_state s;
        gte_pull(cpu, &s);
        plat_gte_write_ctrl(&s, 26, a0);
        gte_push(cpu, &s);
    }
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x8003A3E0);
}

/* ---- OuterProduct12: OP with the rotation diagonal borrowed from the first vector, then restored ---- */

void func_8003A714(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003A714);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x8003A714, jojo_lle_func_8003A714);
    {
        plat_gte_state s; uint32_t keep0, keep2, keep4;
        gte_pull(cpu, &s);
        keep0 = plat_gte_read_ctrl(&s, 0); keep2 = plat_gte_read_ctrl(&s, 2); keep4 = plat_gte_read_ctrl(&s, 4);
        plat_gte_write_ctrl(&s, 0, rd_w(cpu, a0));
        plat_gte_write_ctrl(&s, 2, rd_w(cpu, a0 + 4u));
        plat_gte_write_ctrl(&s, 4, rd_w(cpu, a0 + 8u));
        plat_gte_write_data(&s, 11, rd_w(cpu, a1 + 8u));
        plat_gte_write_data(&s, 9, rd_w(cpu, a1));
        plat_gte_write_data(&s, 10, rd_w(cpu, a1 + 4u));
        plat_gte_command(&s, 0x178000Cu);             /* OP sf=1 lm=0 */
        wr_w(cpu, a2, plat_gte_read_data(&s, 25));
        wr_w(cpu, a2 + 4u, plat_gte_read_data(&s, 26));
        wr_w(cpu, a2 + 8u, plat_gte_read_data(&s, 27));
        plat_gte_write_ctrl(&s, 0, keep0);
        plat_gte_write_ctrl(&s, 2, keep2);
        plat_gte_write_ctrl(&s, 4, keep4);
        gte_push(cpu, &s);
    }
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x8003A714);
}

/* ---- RotTransPers, NormalClip, RotTransPers4: perspective transform and clip on the COP2 state ---- */

void func_8003A7E0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003A7E0);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x8003A7E0, jojo_lle_func_8003A7E0);
    {
        plat_gte_state s; uint32_t sz3;
        gte_pull(cpu, &s);
        plat_gte_write_data(&s, 0, rd_w(cpu, a0));          /* lwc2 VXY0 */
        plat_gte_write_data(&s, 1, rd_w(cpu, a0 + 4u));     /* lwc2 VZ0  */
        plat_gte_command(&s, 0x0180001u);                   /* RTPS sf=1 lm=0 */
        wr_w(cpu, a1, plat_gte_read_data(&s, 14));          /* SXY2 */
        wr_w(cpu, a2, plat_gte_read_data(&s, 8));           /* IR0 */
        sz3 = plat_gte_read_data(&s, 19);
        wr_w(cpu, a3, plat_gte_read_ctrl(&s, 31));          /* FLAG */
        gte_push(cpu, &s);
        cpu->gpr[2] = sar32(sz3, 2u);
    }
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x8003A7E0);
}

void func_8003A810(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003A810);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x8003A810, jojo_lle_func_8003A810);
    {
        plat_gte_state s;
        gte_pull(cpu, &s);
        plat_gte_write_data(&s, 12, a0);
        plat_gte_write_data(&s, 14, a2);
        plat_gte_write_data(&s, 13, a1);
        plat_gte_command(&s, 0x1400006u);                   /* NCLIP */
        cpu->gpr[2] = plat_gte_read_data(&s, 24);           /* MAC0 */
        gte_push(cpu, &s);
    }
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x8003A810);
}

void func_8003A840(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003A840);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x8003A840, jojo_lle_func_8003A840);
    {
        plat_gte_state s; uint32_t sp = cpu->gpr[29], p0, p1, p2, p3, pp, pf, flag1, flag2, sz3;
        gte_pull(cpu, &s);
        plat_gte_write_data(&s, 0, rd_w(cpu, a0));
        plat_gte_write_data(&s, 1, rd_w(cpu, a0 + 4u));
        plat_gte_write_data(&s, 2, rd_w(cpu, a1));
        plat_gte_write_data(&s, 3, rd_w(cpu, a1 + 4u));
        plat_gte_write_data(&s, 4, rd_w(cpu, a2));
        plat_gte_write_data(&s, 5, rd_w(cpu, a2 + 4u));
        plat_gte_command(&s, 0x0280030u);                   /* RTPT sf=1 lm=0 */
        p0 = rd_w(cpu, sp + 16u); p1 = rd_w(cpu, sp + 20u); p2 = rd_w(cpu, sp + 24u);
        wr_w(cpu, p0, plat_gte_read_data(&s, 12));
        wr_w(cpu, p1, plat_gte_read_data(&s, 13));
        wr_w(cpu, p2, plat_gte_read_data(&s, 14));
        flag1 = plat_gte_read_ctrl(&s, 31);
        plat_gte_write_data(&s, 0, rd_w(cpu, a3));
        plat_gte_write_data(&s, 1, rd_w(cpu, a3 + 4u));
        plat_gte_command(&s, 0x0180001u);                   /* RTPS sf=1 lm=0 */
        p3 = rd_w(cpu, sp + 28u); pp = rd_w(cpu, sp + 32u); pf = rd_w(cpu, sp + 36u);
        wr_w(cpu, p3, plat_gte_read_data(&s, 14));
        wr_w(cpu, pp, plat_gte_read_data(&s, 8));
        flag2 = plat_gte_read_ctrl(&s, 31);
        sz3 = plat_gte_read_data(&s, 19);
        wr_w(cpu, pf, flag2 | flag1);
        gte_push(cpu, &s);
        cpu->gpr[2] = sar32(sz3, 2u);
    }
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x8003A840);
}

/* ---- RotMatrix / RotMatrixZYX: 3x3 matrix from three angles, 4096 = 1.0, table 0x8005CE20 ---- */

/* One axis: table entry (cos << 16) | sin for |angle| & 0xFFF; a negative angle negates sin. */
static void gte_axis(CPUState *cpu, uint32_t v, uint32_t *sn, uint32_t *cs) {
    uint32_t neg = v & 0x80000000u;
    uint32_t mag = neg ? 0u - v : v;
    uint32_t e = rd_w(cpu, 0x8005CE20u + 4u * (mag & 0xFFFu));
    uint32_t s = sext16(e);
    *sn = neg ? 0u - s : s;
    *cs = sar32(e, 16u);
}

#define P12(a, b) sar32(mul32((a), (b)), 12u)
#define NP12(a, b) sar32(0u - mul32((a), (b)), 12u)

static void gte_rotmatrix(CPUState *cpu, uint32_t r, uint32_t m, int zyx) {
    uint32_t sx, cx, sy, cy, sz, cz, v15, v16;
    uint32_t vx = rd_h(cpu, r), vy = rd_h(cpu, r + 2u), vz = rd_h(cpu, r + 4u);
    gte_axis(cpu, vx, &sx, &cx);
    gte_axis(cpu, vy, &sy, &cy);
    gte_axis(cpu, vz, &sz, &cz);
    if (!zyx) {
        v15 = P12(cz, 0u - sy);
        v16 = P12(sz, 0u - sy);
        wr_h(cpu, m + 0x00u, P12(cz, cy));
        wr_h(cpu, m + 0x02u, NP12(sz, cy));
        wr_h(cpu, m + 0x04u, sy);
        wr_h(cpu, m + 0x06u, P12(sz, cx) - P12(v15, sx));
        wr_h(cpu, m + 0x08u, P12(cz, cx) + P12(v16, sx));
        wr_h(cpu, m + 0x0Au, NP12(cy, sx));
        wr_h(cpu, m + 0x0Cu, P12(sz, sx) + P12(v15, cx));
        wr_h(cpu, m + 0x0Eu, P12(cz, sx) - P12(v16, cx));
        wr_h(cpu, m + 0x10u, P12(cy, cx));
    } else {
        v15 = P12(sx, sy);
        v16 = P12(sy, cx);
        wr_h(cpu, m + 0x00u, P12(cy, cz));
        wr_h(cpu, m + 0x02u, P12(v15, cz) - P12(sz, cx));
        wr_h(cpu, m + 0x04u, P12(v16, cz) + P12(sx, sz));
        wr_h(cpu, m + 0x06u, P12(sz, cy));
        wr_h(cpu, m + 0x08u, P12(v15, sz) + P12(cx, cz));
        wr_h(cpu, m + 0x0Au, P12(v16, sz) - P12(sx, cz));
        wr_h(cpu, m + 0x0Cu, 0u - sy);
        wr_h(cpu, m + 0x0Eu, P12(sx, cy));
        wr_h(cpu, m + 0x10u, P12(cx, cy));
    }
}

void func_8003A970(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003A970);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x8003A970, jojo_lle_func_8003A970);
    gte_rotmatrix(cpu, a0, a1, 0);
    cpu->gpr[2] = a1;
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x8003A970);
}

void func_8003AC00(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003AC00);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x8003AC00, jojo_lle_func_8003AC00);
    gte_rotmatrix(cpu, a0, a1, 1);
    cpu->gpr[2] = a1;
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x8003AC00);
}

/* ---- ratan2: two table branches; the original's break 6 / break 7 paths fall back to the LLE body ---- */

/* Quotient of one table branch. Returns 0 where the original executes break: divisor 0, or -1 with dividend 0x80000000. */
static int gte_ratan_quot(uint32_t dividend, uint32_t divisor, int32_t *q) {
    int32_t n = (int32_t)dividend, d = (int32_t)divisor;
    if (d == 0) return 0;
    if (d == -1 && n == INT32_MIN) return 0;
    *q = n / d;
    return 1;
}

void func_8003BFA0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003BFA0);
    if (!jojo_piece_on(JOJO_PIECE_GTE)) FALLBACK(0x8003BFA0, jojo_lle_func_8003BFA0);
    {
        uint32_t x = a1, y = a0, v;
        int32_t q = 0;
        int negx = 0, negy = 0, lt;
        if (x & 0x80000000u) { negx = 1; x = 0u - x; }
        if (y & 0x80000000u) { negy = 1; y = 0u - y; }
        if (x == 0u && y == 0u) {
            cpu->gpr[2] = 0u;
            jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x8003BFA0);
            return;
        }
        lt = (int32_t)y < (int32_t)x;
        if (lt) {
            if (y & 0x7FE00000u) {
                if (!gte_ratan_quot(y, sar32(x, 10u), &q)) FALLBACK(0x8003BFA0, jojo_lle_func_8003BFA0);
            } else {
                if (!gte_ratan_quot(y << 10, x, &q)) FALLBACK(0x8003BFA0, jojo_lle_func_8003BFA0);
            }
            v = rd_h(cpu, 0x80060E20u + ((uint32_t)q << 1));
        } else {
            if (x & 0x7FE00000u) {
                if (!gte_ratan_quot(x, sar32(y, 10u), &q)) FALLBACK(0x8003BFA0, jojo_lle_func_8003BFA0);
            } else {
                if (!gte_ratan_quot(x << 10, y, &q)) FALLBACK(0x8003BFA0, jojo_lle_func_8003BFA0);
            }
            v = 1024u - rd_h(cpu, 0x80060E20u + ((uint32_t)q << 1));
        }
        if (negx) v = 2048u - v;
        if (negy) v = 0u - v;
        cpu->gpr[2] = v;
    }
    jojo_cycles_charge(JOJO_GTE_CYCLES); DONE(0x8003BFA0);
}
