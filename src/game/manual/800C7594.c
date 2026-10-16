#include "manual.h"
#include "hex2c_rt.h"

/* 800C7594: twenty records from 80025548(8,50,0), each with index byte +4 = 0..19. $v0 at jr ra is (i < 20) = 0. */
uint32_t man_800C7594(const GuestMem *m) {
    uint32_t v;
    uint32_t i;

    for (i = 0u; i < 20u; i++) {
        v = m->call4(m->ctx, 0x80025548u, 8u, 50u, 0u, 0u);
        if (v != 0u) {
            WR8(v + 4u, i);
        }
    }
    return 0u;
}
