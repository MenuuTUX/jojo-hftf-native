#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FD3FC(const GuestMem *m, uint32_t a0, uint32_t in_v0) {
    uint32_t s0 = a0;
    uint32_t result;

    /* 800FD408: bne v0,zero,800FD458 returns with $v0 unchanged */
    if (in_v0 != 0) {
        return in_v0;
    }
    /* 800FD410: jal 800B428C with $a0 still the original argument */
    m->call4(m->ctx, 0x800B428Cu, a0, 0u, 0u, 0u);
    /* 800FD41C..800FD430: both stores happen before the jal (sh in its delay slot) */
    WR16(s0 + 0xEEu, RD16(s0 + 0xEEu) + 1u);
    WR16(s0 + 0xF0u, RD16(s0 + 0xF0u) + 1u);
    m->call4(m->ctx, 0x80026A04u, s0, 0u, 0u, 0u);
    if (RDS8(s0 + 0x4Du) != -1) {
        return 0xFFFFFFFFu;
    }
    result = RD8(s0 + 5u) + 1u;
    WR8(s0 + 5u, result);
    return result;
}
