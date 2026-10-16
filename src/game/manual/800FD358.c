#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FD358(const GuestMem *m, uint32_t a0, uint32_t in_v0) {
    uint32_t s0 = a0;
    uint32_t s1;
    uint32_t v3;
    uint32_t result;
    int go_inc = 0;

    s1 = RD32(s0 + 0x80u);
    /* 800FD370: bne v0,zero,800FD3E0 returns with $v0 unchanged */
    if (in_v0 != 0) {
        return in_v0;
    }
    /* 800FD378: jal 80026A04 with $a0 still the original argument */
    m->call4(m->ctx, 0x80026A04u, a0, 0u, 0u, 0u);
    if (RD8(s0 + 3u) == 2u) {
        if ((RD32(s1 + 4u) & 0x00FFFF00u) != 0x100u) {
            go_inc = 1;
        } else {
            v3 = RD16(s0 + 0xF0u);
            if (v3 < 0x48u) {
                WR16(s0 + 0xF0u, v3 + 8u);
            }
        }
    }
    if (!go_inc && RDS8(s0 + 0x4Du) != -1) {
        return 0xFFFFFFFFu;
    }
    result = RD8(s0 + 5u) + 1u;
    WR8(s0 + 5u, result);
    return result;
}
