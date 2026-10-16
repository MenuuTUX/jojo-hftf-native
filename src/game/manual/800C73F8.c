#include "manual.h"
#include "hex2c_rt.h"

/* 800C73F8: sixteen records from 80025548(6,2,2), each with a0 at +0x80 and index byte +4 = 0..15.
 * $v0 at jr ra is (i < 16) = 0. */
uint32_t man_800C73F8(const GuestMem *m, uint32_t a0) {
    uint32_t v;
    uint32_t i;

    for (i = 0u; i < 16u; i++) {
        v = m->call4(m->ctx, 0x80025548u, 6u, 2u, 2u, 0u);
        if (v != 0u) {
            WR32(v + 0x80u, a0);
            WR8(v + 4u, i);
        }
    }
    return 0u;
}
