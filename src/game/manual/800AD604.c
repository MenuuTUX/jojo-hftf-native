#include "manual.h"
#include "hex2c_rt.h"

/* 800AD604 (AC_MAIN). $a0 = A (struct), $a1 = B (struct), $a2 = P (word pointer, written by this function).
 * The pseudocode names are shifted: its a1 is $a0, its a2 is $a1, its a3 is $a2. $a3 is overwritten at entry (copy of $a0).
 * Calls to 8003D720 / 8003D1D0 / 8003D3C0 pass $a2 = 2 and $a3 = A, which the code leaves in those registers. */
uint32_t man_800AD604(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2) {
    uint32_t p = a2;
    uint32_t v4 = RD32(a1 + 0x174u);
    uint32_t v6 = RD32(a1 + 0x178u);
    uint32_t v5 = RD32(RD32(a0 + 0x12Cu) + 0x18u) + 48u * RD8(a0 + 0x138u);
    uint32_t tb = RD8(0x800CDD50u + 912u * (uint32_t)RDS8(a1 + 0x136u) + 507u);
    uint32_t v7;
    uint32_t v8;
    uint32_t v9;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r4;
    uint32_t v;

    if (RD8(a0 + 0x1Au) == 2u)
        v7 = (tb == 2u) ? 3u : 1u;
    else if (tb == 2u)
        v7 = RD8(a0 + 0x13Au) + 2u;
    else
        v7 = RD8(a0 + 0x13Au);

    v8 = 0x800CB274u + 8u * v7 + 4u * RD8(a1 + 0x13Au);
    v9 = RD32(v8);

    r1 = m->call4(m->ctx, 0x8003D720u, RD32(p), v9, 2u, a0);
    WR32(p, r1);
    r2 = m->call4(m->ctx, 0x8003D1D0u, v9, 0u, 2u, a0);
    if (r2 == 0u)
        return r2;

    if (RD8(v6 + 517u) == 0u) {
        WR32(p, 0u);
        return 0u;
    }
    if (RD8(v5 + 20u) != 0u) {
        if (RDS8(v6 + 311u) != 8)
            WR8(v6 + 516u, 0u);
        WR8(v4 + 516u, 1u);
        if (RD8(v6 + 517u) != 0u)
            WR16(v6 + 362u, 0xFFFFu);
        WR32(p, 0u);
        return 0xFFFFFFFFu;
    }

    r3 = m->call4(m->ctx, 0x8003D1D0u, r1, 0u, 2u, a0);
    if (r3 == 0u)
        return r3;
    r4 = m->call4(m->ctx, 0x8003D3C0u, r1, 0u, 2u, a0);
    v = r4 << 16;
    if ((int32_t)v > 0)
        return v;
    WR32(p, 0x3F800000u);
    return 0x3F800000u;
}
