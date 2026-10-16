#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800BC20C(const GuestMem *m, uint32_t a0);

/* One function for the whole body 800A4BFC .. jr ra at 800A57E8 (epilogue 800A57C8). The IDA pieces 800A4DC0, 800A50A4,
 * 800A517C, 800A53D8 and 800A549C are written inline here. Other code also jal's into 800A4DC0, 800A50A4, 800A517C,
 * 800A53D8, 800A549C and 800A54F0 with its own register state; those entries are not covered by this function.
 *
 * The body is a chain of stages. Stage n tests the flag word RD32(s2) (s2 = 0x80066764 + 32 * RD8(RD32(0x800A6084) + 4))
 * for one bit, and a condition on the game state; when the bit is clear and the condition holds, it bumps the counter
 * s0, the byte at 0x80066CC2, and sets the bit in the word at 0x80066CB8. After most stages, if (int8)s0 >= 10 the
 * function leaves with $v0 = 0. The stage at 800A5770 returns the value it tested or the counter byte.
 *
 * Inputs: $a1 is read (record pointer at 800A4DC0 and 800A4E00). $a2 and $a3 are never written, so they reach the
 * jal 800BC20C unchanged (that callee takes only $a0). $a0 is written before any read. $v0 is never read before a write.
 *
 * The record s3 = 0x800CDD50 + 912 * RD8(0x80066CA4); s4 = RD32(s3 + 0x17C); s6 = RD32(s3 + 0x180); s5 = 0x800A60A8.
 */
uint32_t man_800A4BFC(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t s0;                 /* counter: only its low byte, read as (int8_t), matters; increments are 32-bit */
    uint32_t s2;                 /* flag record, 32 bytes at 0x80066764 */
    uint32_t s3;                 /* 912-byte record at 0x800CDD50 */
    uint32_t s4;
    uint32_t s5;
    uint32_t s6;
    uint32_t r;                  /* $v0 returned by 800BC20C (also $a0 in the stage at 800A4E78) */
    uint32_t v0 = 0u;            /* $v0 at jr ra; set before every goto done */

    s3 = 0x800CDD50u + 912u * (uint32_t)RD8(0x80066CA4u);
    s2 = 0x80066764u + ((uint32_t)RD8(RD32(0x800A6084u) + 4u) << 5);
    s4 = RD32(s3 + 0x17Cu);
    s5 = 0x800A60A8u;
    s6 = RD32(s3 + 0x180u);

/* A stage bump: s0++, counter byte at 0x80066CC2 ++, flag bit set in the word at 0x80066CB8 */
#define BUMP(bit) do { \
        s0 += 1u; \
        WR8(0x80066CC2u, RD8(0x80066CC2u) + 1u); \
        WR32(0x80066CB8u, RD32(0x80066CB8u) | (uint32_t)(bit)); \
    } while (0)

/* Exit when the counter low byte, signed, is 10 or more; $v0 is 0 there (slti result) */
#define EXIT_IF_S0_GE10() do { \
        if ((int8_t)s0 >= 10) { \
            v0 = 0u; \
            goto done; \
        } \
    } while (0)

    /* 800A4C74..800A4C80: exit with $v0 = byte 0x80066CAF unless the record byte matches it */
    if ((int32_t)RDS8(s3 + 0x136u) != (int32_t)RD8(0x80066CAFu)) {
        v0 = RD8(0x80066CAFu);
        goto done;
    }

    /* 800A4C88..800A4C94: s0 = signed byte at s2+8, exit if >= 10 */
    s0 = (uint32_t)(int32_t)RDS8(s2 + 8u);
    EXIT_IF_S0_GE10();

    /* stage 1: bit 0x1, RD8(s4 + 0x158) != 0 */
    if ((RD32(s2) & 0x1u) == 0u && RD8(s4 + 0x158u) != 0u) {
        BUMP(0x1u);
    }
    EXIT_IF_S0_GE10();

    /* stage 2: bit 0x8, byte at (RD32(s3 + 0x130) + 0x22) == 0 */
    if ((RD32(s2) & 0x8u) == 0u && RD8(RD32(s3 + 0x130u) + 0x22u) == 0u) {
        BUMP(0x8u);
    }
    EXIT_IF_S0_GE10();

    /* stage 3: bit 0x10, byte at (RD32(s3 + 0x130) + 0x22) == 10 */
    if ((RD32(s2) & 0x10u) == 0u && RD8(RD32(s3 + 0x130u) + 0x22u) == 10u) {
        BUMP(0x10u);
    }
    EXIT_IF_S0_GE10();

    /* stage 4 (800A4DAC..800A4DF8): bit 0x1000, signed byte at a1 + 0x10 == 11 */
    if ((RD32(s2) & 0x1000u) == 0u && (int32_t)RDS8(a1 + 0x10u) == 11) {
        BUMP(0x1000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 5 (800A4E00..800A4E54): r = 800BC20C(signed byte at a1 + 0x10), always called here;
     * bit 0x4000, (r & 0xF) == 7 */
    r = man_800BC20C(m, (uint32_t)(int32_t)RDS8(a1 + 0x10u));
    if ((RD32(s2) & 0x4000u) == 0u && (r & 0xFu) == 7u) {
        BUMP(0x4000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 6 (800A4E64..800A4EB8): bit 0x2000, (int8)r != 0 and (r & 0xF) == ((r >> 4) & 0xF) */
    if ((RD32(s2) & 0x2000u) == 0u && (r & 0xFFu) != 0u && (r & 0xFu) == ((r >> 4) & 0xFu)) {
        BUMP(0x2000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 7 (800A4EC8..800A4F0C): bit 0x8000, signed halfword at s3 + 0x16A == 72 */
    if ((RD32(s2) & 0x8000u) == 0u && (int32_t)RDS16(s3 + 0x16Au) == 72) {
        BUMP(0x8000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 8 (800A4F18..800A4F60): bit 0x10000, signed halfword at s3 + 0x16A == 1 */
    if ((RD32(s2) & 0x10000u) == 0u && (int32_t)RDS16(s3 + 0x16Au) == 1) {
        BUMP(0x10000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 9 (800A4F6C..800A4FB8): bit 0x200000, byte at 0x80066CAE != 0 */
    if ((RD32(s2) & 0x200000u) == 0u && RD8(0x80066CAEu) != 0u) {
        BUMP(0x200000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 10 (800A4FC0..800A5018): bit 0x20000, signed byte at (RD32(s4 + 0x130) + 0x2A) >= 20 */
    if ((RD32(s2) & 0x20000u) == 0u && (int32_t)RDS8(RD32(s4 + 0x130u) + 0x2Au) >= 20) {
        BUMP(0x20000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 11 (800A5020..800A5078): bit 0x40000, same byte >= 30 */
    if ((RD32(s2) & 0x40000u) == 0u && (int32_t)RDS8(RD32(s4 + 0x130u) + 0x2Au) >= 30) {
        BUMP(0x40000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 12 (800A5080..800A50DC): bit 0x80000, same byte >= 40 */
    if ((RD32(s2) & 0x80000u) == 0u && (int32_t)RDS8(RD32(s4 + 0x130u) + 0x2Au) >= 40) {
        BUMP(0x80000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 13 (800A50E0..800A513C): bit 0x100000, same byte >= 50 */
    if ((RD32(s2) & 0x100000u) == 0u && (int32_t)RDS8(RD32(s4 + 0x130u) + 0x2Au) >= 50) {
        BUMP(0x100000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 14 (800A5140..800A518C): bit 0x800000, signed byte at s5 + 0 >= 3 */
    if ((RD32(s2) & 0x800000u) == 0u && (int32_t)RDS8(s5 + 0x0u) >= 3) {
        BUMP(0x800000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 15 (800A5198..800A51E8): bit 0x1000000, signed byte at s5 + 7 >= 5 */
    if ((RD32(s2) & 0x1000000u) == 0u && (int32_t)RDS8(s5 + 0x7u) >= 5) {
        BUMP(0x1000000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 16 (800A51F0..800A5238): bit 0x2000000, signed byte at s5 + 8 != 0 */
    if ((RD32(s2) & 0x2000000u) == 0u && (int32_t)RDS8(s5 + 0x8u) != 0) {
        BUMP(0x2000000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 17 (800A5244..800A5290): bit 0x8000000, signed byte at s5 + 0xA >= 3 */
    if ((RD32(s2) & 0x8000000u) == 0u && (int32_t)RDS8(s5 + 0xAu) >= 3) {
        BUMP(0x8000000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 18 (800A529C..800A52E4): bit 0x10000000, signed byte at s5 + 0xB == 0 */
    if ((RD32(s2) & 0x10000000u) == 0u && (int32_t)RDS8(s5 + 0xBu) == 0) {
        BUMP(0x10000000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 19 (800A52F0..800A5338): bit 0x20000000, signed byte at s5 + 9 == 0 */
    if ((RD32(s2) & 0x20000000u) == 0u && (int32_t)RDS8(s5 + 0x9u) == 0) {
        BUMP(0x20000000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 20 (800A5344..800A538C): bit 0x4000000, signed byte at s5 + 0xC != 0 */
    if ((RD32(s2) & 0x4000000u) == 0u && (int32_t)RDS8(s5 + 0xCu) != 0) {
        BUMP(0x4000000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 21 (800A5398..800A53E0): bit 0x80, signed byte at s5 + 5 == 0 */
    if ((RD32(s2) & 0x80u) == 0u && (int32_t)RDS8(s5 + 0x5u) == 0) {
        BUMP(0x80u);
    }
    EXIT_IF_S0_GE10();

    /* stage 22 (800A53F0..800A5434, the piece 800A53D8 onward): bit 0x100, signed byte at s5 + 4 == 0 */
    if ((RD32(s2) & 0x100u) == 0u && (int32_t)RDS8(s5 + 0x4u) == 0) {
        BUMP(0x100u);
    }
    EXIT_IF_S0_GE10();

    /* stage 23 (800A5444..800A5488): bit 0x200, signed byte at s5 + 3 == 0 */
    if ((RD32(s2) & 0x200u) == 0u && (int32_t)RDS8(s5 + 0x3u) == 0) {
        BUMP(0x200u);
    }
    EXIT_IF_S0_GE10();

    /* stage 24 (800A549C..800A54E4, the piece 800A549C): bit 0x400, signed byte at s5 + 2 == 0 */
    if ((RD32(s2) & 0x400u) == 0u && (int32_t)RDS8(s5 + 0x2u) == 0) {
        BUMP(0x400u);
    }
    EXIT_IF_S0_GE10();

    /* stage 25 (800A54EC..800A5538): bit 0x800, signed byte at s5 + 1 == 0 */
    if ((RD32(s2) & 0x800u) == 0u && (int32_t)RDS8(s5 + 0x1u) == 0) {
        BUMP(0x800u);
    }
    EXIT_IF_S0_GE10();

    /* stage 26 (800A5540..800A5594): bit 0x80000000 clear (bltz skip), byte at (RD32(s4 + 0x130) + 0x27) >= 5 unsigned */
    if ((RD32(s2) & 0x80000000u) == 0u && RD8(RD32(s4 + 0x130u) + 0x27u) >= 5u) {
        BUMP(0x80000000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 27 (800A5598..800A55F4): bit 0x40000000, same byte >= 10 unsigned */
    if ((RD32(s2) & 0x40000000u) == 0u && RD8(RD32(s4 + 0x130u) + 0x27u) >= 10u) {
        BUMP(0x40000000u);
    }
    EXIT_IF_S0_GE10();

    /* stage 28 (800A55FC..800A5648): bit 0x40, signed byte at s5 + 0xD != 0 */
    if ((RD32(s2) & 0x40u) == 0u && (int32_t)RDS8(s5 + 0xDu) != 0) {
        BUMP(0x40u);
    }
    EXIT_IF_S0_GE10();

    /* stage 29 (800A5650..800A56AC): bit 0x2, byte at s3 + 0x203 != 0 and byte at 0x80066CAE == 0 */
    if ((RD32(s2) & 0x2u) == 0u && RD8(s3 + 0x203u) != 0u && RD8(0x80066CAEu) == 0u) {
        BUMP(0x2u);
    }
    EXIT_IF_S0_GE10();

    /* stage 30 (800A56B4..800A5700): bit 0x4, signed byte at s5 + 0x10 != 0 */
    if ((RD32(s2) & 0x4u) == 0u && (int32_t)RDS8(s5 + 0x10u) != 0) {
        BUMP(0x4u);
    }
    EXIT_IF_S0_GE10();

    /* stage 31 (800A5708..800A576C): bit 0x20, byte at (RD32(s6 + 0x130) + 0x33) != 3 and signed halfword at s6 + 0x16A <= 0 */
    if ((RD32(s2) & 0x20u) == 0u && RD8(RD32(s6 + 0x130u) + 0x33u) != 3u && (int32_t)RDS16(s6 + 0x16Au) <= 0) {
        BUMP(0x20u);
    }
    EXIT_IF_S0_GE10();

    /* stage 32 (800A5770..800A57C4): $a1 = 0x400000. If that bit is set in the record, return it (the and result).
     * Else, if the byte at 0x800D0880 + 68 * signed byte(s3 + 0x136) + 0x37 is nonzero, the counter byte is bumped
     * and the bit set; $v0 is the counter byte before the store, plus 1 (no s0 bump). Else $v0 = 0. */
    if ((RD32(s2) & 0x400000u) != 0u) {
        v0 = RD32(s2) & 0x400000u;
        goto done;
    }
    if (RD8(0x800D0880u + 68u * (uint32_t)(int32_t)RDS8(s3 + 0x136u) + 0x37u) != 0u) {
        v0 = RD8(0x80066CC2u) + 1u;
        WR8(0x80066CC2u, v0);
        WR32(0x80066CB8u, RD32(0x80066CB8u) | 0x400000u);
    } else {
        v0 = 0u;
    }

done:
    /* 800A57C8..800A57E8: epilogue; the restores of ra and s0..s6 need no code */
    return v0;

#undef BUMP
#undef EXIT_IF_S0_GE10
}
