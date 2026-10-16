#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_80017C74(const GuestMem *m, uint32_t a0, uint32_t a1) {
    int32_t idx = (int32_t)(int16_t)(a1 & 0xFFFFu);
    uint32_t e = 0x8005631Cu + (uint32_t)idx * 12u;
    uint32_t lo = RD32(e);
    uint32_t hi = RD32(e + 4u);

    WR32(a0 + 0x18u, hi);
    WR32(a0 + 0x0Cu, lo);
    return m->call4(m->ctx, 0x8004CB80u, lo, a0 + 8u, a0, 0u);
}
