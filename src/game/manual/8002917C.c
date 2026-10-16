#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_8002917C(const GuestMem *m) {
    uint32_t p;

    WR8(0x8008EBC1u, 0u);

    /* pointer at 0x8005AA48 is reloaded before each store group, as in the MIPS */
    p = RD32(0x8005AA48u);
    WR8(p + 8u, RD8(p + 10u));

    p = RD32(0x8005AA48u);
    WR8(p + 0u, RD8(p + 0u) - 1u);

    p = RD32(0x8005AA48u);
    WR8(p + 1u, 1u);

    p = RD32(0x8005AA48u);
    WR8(p + 3u, 0u);
    WR8(p + 2u, 0u);

    /* 800291D4..800291DC: a0 = byte 0x8006CE58, jal 800173E4 */
    return m->call4(m->ctx, 0x800173E4u, RD8(0x8006CE58u), 0u, 0u, 0u);
}
