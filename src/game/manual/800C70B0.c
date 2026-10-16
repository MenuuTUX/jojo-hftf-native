#include "manual.h"
#include "hex2c_rt.h"

/* 800C70B0: one record from 80025548(6,2,4) gets flag byte +0x12 = 1; then ten more, each with index byte +4 = 0..9.
 * Every record gets a0 at +0x80. $v0 at jr ra is the last sltiu of the loop: (i < 10) = 0. */
uint32_t man_800C70B0(const GuestMem *m, uint32_t a0) {
    uint32_t v;
    uint32_t i;

    v = m->call4(m->ctx, 0x80025548u, 6u, 2u, 4u, 0u);
    if (v != 0u) {
        WR32(v + 0x80u, a0);
        WR8(v + 0x12u, 1u);
    }
    for (i = 0u; i < 10u; i++) {
        v = m->call4(m->ctx, 0x80025548u, 6u, 2u, 4u, 0u);
        if (v != 0u) {
            WR32(v + 0x80u, a0);
            WR8(v + 4u, i);
        }
    }
    return 0u;
}
