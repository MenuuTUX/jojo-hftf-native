#include "manual.h"
#include "hex2c_rt.h"

/* 800AD994 (AC_MAIN). $a0 = A (struct), $a1 = B (struct), $a2 = P (word pointer, written by this function).
 * Calls pass $a2 = P (never written here) and $a3 = 0 (not an argument of this function, not modelled). */
uint32_t man_800AD994(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2) {
    uint32_t r0;
    uint32_t v7;
    uint32_t v8;
    uint32_t v9;
    uint32_t r1;
    uint32_t r2;
    uint32_t r4;
    uint32_t v;

    r0 = m->call4(m->ctx, 0x8003D1D0u, RD32(a2), 0u, a2, 0u);
    if (r0 == 0u)
        return r0;

    if (RD8(a0 + 0x1Au) == 2u)
        v7 = (RD8(a1 + 0x1FBu) == 2u) ? 3u : 1u;
    else if (RD8(a1 + 0x1FBu) == 2u)
        v7 = RD8(a0 + 0x13Au) + 2u;
    else
        v7 = RD8(a0 + 0x13Au);

    v8 = 0x800CB294u + 8u * v7 + 4u * RD8(a1 + 0x13Au);
    v9 = RD32(v8);

    r1 = m->call4(m->ctx, 0x8003D720u, RD32(a2), v9, a2, 0u);
    WR32(a2, r1);
    r2 = m->call4(m->ctx, 0x8003D1D0u, v9, 0u, a2, 0u);
    if (r2 == 0u)
        return r2;

    r4 = m->call4(m->ctx, 0x8003D3C0u, r1, 0u, a2, 0u);
    v = r4 << 16;
    if ((int32_t)v > 0)
        return v;
    WR32(a2, 0x3F800000u);
    return 0x3F800000u;
}
