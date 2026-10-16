#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_80026B30(const GuestMem *m, uint32_t a0) {
    uint32_t idx = RD8(a0 + 0xB1u);
    uint32_t t;
    uint32_t b0;
    uint32_t b1;

    WR32(a0 + 0xA0u, RD32(0x80089FB0u + idx * 4u));
    WR32(a0 + 0xA4u, RD32(0x80089F70u + idx * 4u));

    t = 0x80089F50u + idx * 2u;
    b0 = RD8(t);
    WR8(a0 + 0xB2u, b0);
    b1 = RD8(t + 1u);
    WR8(a0 + 0xB3u, b1);
    return b1;
}
