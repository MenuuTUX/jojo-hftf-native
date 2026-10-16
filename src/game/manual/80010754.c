#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_80010754(const GuestMem *m) {
    uint32_t a2 = RD32(0x110u);
    uint32_t v1 = 0x801FC400u;
    uint32_t a0 = 0x801FEC00u;
    uint32_t a1 = 0u;
    uint32_t v0;

    do {
        a2 += 192u;
        WR16(v1, 0u);
        WR32(v1 + 8u, a0);
        v1 += 128u;
        a0 += 2048u;
        a1 += 1u;
        v0 = ((int32_t)a1 < 3) ? 1u : 0u;
        WR32(a2 + 0x94u, 0x40000404u);
    } while (v0 != 0u);
    return v0;
}
