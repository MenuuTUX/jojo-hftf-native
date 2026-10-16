#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FD244(const GuestMem *m, uint32_t a0, uint32_t in_v0) {
    uint32_t s1 = a0;
    uint32_t s0;
    uint32_t v1;
    int32_t v7;
    uint32_t result;

    /* 800FD258..800FD264: $v0 is read at entry (beq v0,zero at 800FD25C) */
    if (in_v0 != 0) {
        WR8(s1 + 7u, 1u);
        return 1u;
    }
    s0 = RD32(s1 + 0x80u);
    v1 = RD8(s1 + 4u);
    if (v1 == 2u || v1 == 25u || v1 == 4u || v1 == 6u || v1 == 0u) {
        if (RD8(s1 + 7u) == 0u) {
            WR16(s1 + 106u, RD16(s0 + 106u));
            WR16(s1 + 110u, RD16(s0 + 110u));
        }
        m->call4(m->ctx, 0x80026A04u, s1, 0u, 0u, 0u);
        v7 = RDS8(s1 + 77u);
    } else {
        m->call4(m->ctx, 0x80026A04u, s1, 0u, 0u, 0u);
        v7 = RDS8(s1 + 77u);
        WR16(s1 + 110u, RD16(s1 + 110u) + 1u);
    }
    if (v7 != -1 && RD8(s0 + 6u) == 0u && RD8(s0 + 517u) != 0u && RD8(s0 + 506u) == 9u) {
        return 9u;
    }
    result = RD8(s1 + 5u) + 1u;
    WR8(s1 + 5u, result);
    return result;
}
