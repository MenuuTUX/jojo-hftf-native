#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800121F0(const GuestMem *m, uint32_t a0, uint32_t a1);

/* 800A2874 (GAMESS). $a0 = record pointer: s1 = signed halfword at +0x0C, s2 = signed halfword at +0x0E.
 * Writes the text "00'00  " digits into the buffer at 0x800A3608:
 *   0x800A3608 = s1/10 + 48 (not reduced; the byte store keeps the low byte)
 *   0x800A3609 = s1 - 10*(s1/10) + 48
 *   0x800A360B = s3/10 + 48, with s3 = (int16)((5*s2)/3)
 *   0x800A360C = s3 - 10*(s3/10) + 48
 *   0x800A360D = 0
 * The divisions are the original's multiply-high sequences (0x66666667 for /10, 0x55555556 for /3) with the sign fix-up.
 * Then the record at sp+0x10 (172, 96, 3, two zero bytes) goes to man_800121F0 as $a0, with $a1 = 0x800A3608. The
 * original's $a2 and $a3 are set by the digit code, but the callee reads only $a0 and $a1. The digit stores happen before
 * the s2 read, as in the MIPS. */
uint32_t man_800A2874(const GuestMem *m, uint32_t a0) {
    uint32_t sp = m->sp_get(m->ctx) - 0x20u;   /* the original "addiu sp,sp,-32" */
    uint32_t r;
    int32_t s1, m1, q1, s2, v2, m2, h2, s3, m3, q3;

    s1 = RDS16(a0 + 0x0Cu);
    m1 = (s1 < 0) ? -1 : 0;                                            /* sra 31 of the halfword */
    q1 = (int32_t)(((int64_t)s1 * 0x66666667LL) >> 32) >> 2;           /* mult/mfhi/sra 2 */
    WR8(0x800A3608u, q1 - m1 + 48);                                    /* 800A28CC */
    WR8(0x800A3609u, s1 - 10 * (q1 - m1) + 48);                        /* 800A28F0 */

    s2 = RDS16(a0 + 0x0Eu);                                            /* 800A28F4 */
    v2 = 5 * s2;
    m2 = (v2 < 0) ? -1 : 0;
    h2 = (int32_t)(((int64_t)v2 * 0x55555556LL) >> 32);                /* mult v2, 0x55555556 */
    s3 = (int16_t)(h2 - m2);                                           /* the 16-bit truncation: sll/sra 16 */
    m3 = (s3 < 0) ? -1 : 0;
    q3 = (int32_t)(((int64_t)s3 * 0x66666667LL) >> 32) >> 2;
    WR8(0x800A360Bu, q3 - m3 + 48);                                    /* 800A293C */
    WR8(0x800A360Cu, s3 - 10 * (q3 - m3) + 48);                        /* 800A295C */
    WR8(0x800A360Du, 0u);                                              /* 800A2960 */

    m->sp_set(m->ctx, sp);
    WR16(sp + 0x10u, 172u);
    WR16(sp + 0x12u, 96u);
    WR16(sp + 0x14u, 3u);
    WR8(sp + 0x16u, 0u);
    WR8(sp + 0x17u, 0u);
    r = man_800121F0(m, sp + 0x10u, 0x800A3608u);                      /* 800A2980 */
    m->sp_set(m->ctx, sp + 0x20u);
    return r;
}
