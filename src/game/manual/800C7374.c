#include "manual.h"
#include "hex2c_rt.h"

/* 800C7374: eight records from 80025548(6,2,0), each with a0 at +0x80, index byte +4 = 0..7, and halfword +0xB4 = a1,
 * or -a1 when byte +0xB of a0 is 0 (re-read on every iteration). $v0 at jr ra is (i < 8) = 0. */
uint32_t man_800C7374(const GuestMem *m, uint32_t a0, uint32_t a1) {
    uint32_t v;
    uint32_t i;

    for (i = 0u; i < 8u; i++) {
        v = m->call4(m->ctx, 0x80025548u, 6u, 2u, 0u, 0u);
        if (v != 0u) {
            WR32(v + 0x80u, a0);
            WR16(v + 0xB4u, (RD8(a0 + 0xBu) == 0u) ? (0u - a1) : a1);
            WR8(v + 4u, i);
        }
    }
    return 0u;
}
