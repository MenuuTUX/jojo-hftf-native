#include "manual.h"
#include "hex2c_rt.h"

/* 800C7CE8: reads $v0 at entry (andi v0,v0,0x7f at 800C7D04). Nonzero masked value is returned as is. Otherwise one
 * record from 80025548(6,2,8): if it is 0 the function returns 0; else byte +5 = 1, halfword +0xC = a0, and the result
 * is 800265B0(a1, a2 & 0xff) (800265B0 reads only a0 and a1 before writing a2/a3, so those are passed as 0). */
uint32_t man_800C7CE8(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t in_v0) {
    uint32_t v;
    uint32_t r;

    v = in_v0 & 0x7Fu;
    if (v != 0u) {
        return v;
    }
    r = m->call4(m->ctx, 0x80025548u, 6u, 2u, 8u, 0u);
    if (r == 0u) {
        return 0u;
    }
    WR8(r + 5u, 1u);
    WR16(r + 12u, a0);
    return m->call4(m->ctx, 0x800265B0u, a1, a2 & 0xFFu, 0u, 0u);
}
