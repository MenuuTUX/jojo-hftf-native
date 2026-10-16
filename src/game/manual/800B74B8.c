#include "manual.h"
#include "hex2c_rt.h"

/* 800B74B8 (AC_MAIN). $a0 = A (struct), $a1 = B (struct). Returns 0.
 * s0 = entry pointer at 0x800DAD10 + 160*(signed byte A+0x136) + 68 (word index 40*x+17 of the table).
 * Calls 800B7260(A, B) first, then reads the 16-bit values and maybe calls 800B89D8(A, B).
 * The callees read $a2/$a3 only after writing them, so they are passed as 0. */
uint32_t man_800B74B8(const GuestMem *m, uint32_t a0, uint32_t a1) {
    uint32_t s1 = a0;
    uint32_t s2 = a1;
    uint32_t s0;
    uint32_t p;
    uint32_t v5;
    uint32_t d;
    uint32_t v6;

    s0 = RD32(0x800DAD10u + 160u * (uint32_t)RDS8(s1 + 0x136u) + 68u);
    m->call4(m->ctx, 0x800B7260u, s1, s2, 0u, 0u);

    p = RD32(0x800DAD08u);
    v5 = ((uint32_t)RD8(p + 1u) << 8) | (uint32_t)RD8(p + 2u);

    d = (uint32_t)RD16(s1 + 106u) - (uint32_t)RD16(s0 + 106u);
    if (((d << 16) & 0x80000000u) != 0u)
        d = 0u - d;
    v6 = d & 0xFFFFu;

    if (v5 < v6)
        m->call4(m->ctx, 0x800B89D8u, s1, s2, 0u, 0u);

    return 0u;
}
