#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FB904(const GuestMem *m, uint32_t a0) {
    uint32_t s1 = a0;
    uint32_t s0 = 0;
    uint32_t v;
    do {
        v = m->call4(m->ctx, 0x80025548u, (uint32_t)(RDS8(s1 + 0x136u) + 4), 0u, 19u, 0u);
        if (v != 0) {
            WR32(v + 0x80u, s1);
            WR8(v + 4u, s0);
        }
        s0 = (s0 + 1u) & 0xffu;
    } while ((s0 & 0xffu) < 4u);
    return 0u;
}
