#include "manual.h"
#include "hex2c_rt.h"

/* 800CAB90: decrements the halfword +0xC. When the new value is -1 (as int16), sets byte +1 = 1, increments byte +6,
 * and returns the old byte +6 plus 1. Otherwise returns the new halfword sign-extended. */
uint32_t man_800CAB90(const GuestMem *m, uint32_t a0) {
    uint32_t v;
    int32_t r;
    uint32_t old;

    v = (uint32_t)(RD16(a0 + 12u) - 1u);
    WR16(a0 + 12u, v);
    r = (int16_t)v;
    if (r == -1) {
        WR8(a0 + 1u, 1u);
        old = RD8(a0 + 6u);
        WR8(a0 + 6u, old + 1u);
        return old + 1u;
    }
    return (uint32_t)r;
}
