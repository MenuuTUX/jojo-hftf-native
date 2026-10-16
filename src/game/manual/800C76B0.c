#include "manual.h"
#include "hex2c_rt.h"

/* 800C76B0 (AC_MAIN), 800C76B0..800C7774 (jr ra at 800C7770, epilogue loads ra/s0/s1 and addiu sp,32).
 * Frame: sp -= 0x20 (the original "addiu sp,sp,-32"); the saved s0, s1, ra slots need no code.
 *
 * Calls and the registers each one gets (from the MIPS, not the pseudocode):
 *  - jal 800C7C34 (800C76C0): $a0 = the incoming $a0 (copied to $s1 at 800C76B8, not changed before the jal).
 *    $a1, $a2, $a3 are not written before the jal, so they pass on the incoming values.
 *  - jal 80025548 (800C76D4, in the loop): $a0 = 6, $a1 = 2 (800C76D0), $a2 = 7 (delay slot 800C76D8),
 *    $a3 = the incoming $a3 (never written in this function).
 *  - jal 8002468C (800C775C): delay slot 800C7760 sets $a0 = $s1 = the incoming $a0.
 *    $a1 = $v0 of the last 80025548 call (800C76DC, "addu a1,v0,zero", runs every iteration; the second
 *    iteration is the last one). $a2 = 7 (set by the delay slot of the last 80025548 call, not written after).
 *    $a3 = the incoming $a3.
 *
 * The loop runs exactly twice (s0 = 0, 1). Its body stores into the object returned by 80025548 when that is not 0.
 * Returns the $v0 that 8002468C leaves (the epilogue writes only ra, s1, s0 and sp).
 * Globals: 0x800DAE70 (word, stored at +0x68), 0x800DAE74 (word, stored at +0x6C).
 * The incoming $a0 is the record pointer (the pseudocode's a1): read at +0x13, +0x0B, +0x147; stored at +0x80. */
uint32_t man_800C76B0(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp;
    uint16_t i;
    uint32_t v3;                 /* $v0 of 80025548 (pseudocode v3); also the final $v0 of 8002468C */
    uint32_t v4;                 /* pseudocode v4 (word at 0x800DAE74) */
    uint32_t a1_reg;             /* $a1 as the MIPS leaves it: the last 80025548 return */

    /* 800C76B8..800C76C0: sp frame, then sub_800C7C34 with the incoming $a0..$a3 */
    sp = m->sp_get(m->ctx) - 0x20u;
    m->sp_set(m->ctx, sp);
    m->call4(m->ctx, 0x800C7C34u, a0, a1, a2, a3);

    /* 800C76C8..800C7758: two passes of sub_80025548(6, 2, 7) and the store block when the result is not 0 */
    for (i = 0u; i < 2u; ++i) {
        v3 = m->call4(m->ctx, 0x80025548u, 6u, 2u, 7u, a3);
        a1_reg = v3;                                    /* 800C76DC: addu a1,v0,zero */
        if (v3 != 0u) {
            /* 800C76E8..800C76F0: byte at record+0x13 -> v3+19 */
            WR8(v3 + 19u, RD8(a0 + 19u));
            /* 800C76F4..800C76FC: byte at record+0x0B -> v3+11 */
            WR8(v3 + 11u, RD8(a0 + 11u));
            /* 800C7700..800C770C: word 0x800DAE70 -> v3+104 */
            WR32(v3 + 104u, RD32(0x800DAE70u));
            /* 800C7710..800C7720: word 0x800DAE74 loaded into $a0 (v4); record pointer -> v3+128 */
            v4 = RD32(0x800DAE74u);
            WR32(v3 + 128u, a0);
            /* 800C7718..800C7724: 3 * i (sll 1 + addu i), byte -> v3+18 */
            WR8(v3 + 18u, 3u * (uint32_t)i);
            /* 800C7728: word v4 -> v3+108 */
            WR32(v3 + 108u, v4);
            /* 800C772C..800C7734: byte at record+0x0B -> v3+11 again */
            WR8(v3 + 11u, RD8(a0 + 11u));
            /* 800C7738..800C7744: byte at record+0x147, & 3 -> v3+4 */
            WR8(v3 + 4u, RD8(a0 + 0x147u) & 3u);
        }
    }

    /* 800C775C: sub_8002468C(a0 = record, a1 = last $v0 of 80025548, a2 = 7, a3 = incoming $a3) */
    v3 = m->call4(m->ctx, 0x8002468Cu, a0, a1_reg, 7u, a3);
    m->sp_set(m->ctx, sp + 0x20u);
    return v3;
}
