#include "manual.h"
#include "hex2c_rt.h"

/* 80025E24: a0 is passed unchanged to 80026768(a0, s0) with s0 = a0 + 0x40000 (a2, a3 pass on). Then LoadImage on the
 * block at s0+8 (plus the word at s0+8 when bit 3 of the word at s0+4 is set); when that bit is set also LoadImage on
 * the second rect/data pair (s0+12, s0+20). Ends with DrawSync(0) and returns its $v0. */
uint32_t man_80025E24(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t s0 = a0 + 0x40000u;
    uint32_t v4, v2, rect, p;

    m->call4(m->ctx, 0x80026768u, a0, s0, a2, a3);
    v4 = RD32(s0 + 4u);
    v2 = s0 + 8u;
    if ((v4 & 8u) != 0u) {
        rect = s0 + 12u;
        p = s0 + 20u;
        v2 += RD32(s0 + 8u);
    }
    m->call4(m->ctx, 0x80036348u, v2 + 4u, v2 + 12u, 0u, 0u);
    if ((v4 & 8u) != 0u) {
        m->call4(m->ctx, 0x80036348u, rect, p, 0u, 0u);
    }
    return m->call4(m->ctx, 0x8003609Cu, 0u, 0u, 0u, 0u);
}
