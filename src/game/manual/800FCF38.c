#include "manual.h"
#include "hex2c_rt.h"

/* 800FCF38 (PL02X). $a0 = actor (A). Reads $a0 only. Calls 800268BC with $a0 = A, $a1 = table word, and the $a2 = A and
 * $a3 = 0x800FFACC this code leaves in those registers (800268BC does not read $a2 or $a3). Returns the callee's $v0. */
uint32_t man_800FCF38(const GuestMem *m, uint32_t a0) {
    uint32_t actor = a0;
    uint32_t st;
    uint32_t idx;
    uint32_t t;
    uint32_t p;
    uint8_t v;

    WR8(actor + 6u, 1u);
    idx = RD8(actor + 4u);
    st = RD32(actor + 0x80u);
    WR8(actor + 50u, RD8(st + 50u) - RD8(0x800F4904u + idx));
    WR16(actor + 110u, RD16(actor + 110u) + RD16(0x800FFACCu + 4u * idx + 2u));
    t = 0x800FFADCu + 16u * idx;
    WR32(actor + 116u, RD32(t + 8u));
    WR32(actor + 124u, RD32(t + 12u));
    v = RD8(st + 11u);
    WR8(actor + 11u, v);
    if (v != 0u) {
        WR16(actor + 106u, RD16(actor + 106u) - RD16(0x800FFACCu + 4u * idx));
        WR32(actor + 112u, RD32(t + 0u));
        WR32(actor + 120u, RD32(t + 4u));
    } else {
        WR16(actor + 106u, RD16(actor + 106u) + RD16(0x800FFACCu + 4u * idx));
        WR32(actor + 112u, 0u - RD32(t + 0u));
        WR32(actor + 120u, 0u - RD32(t + 4u));
    }
    p = 0x800FFAC4u + 2u * idx;
    WR16(actor + 238u, RDS8(p));
    WR16(actor + 240u, RDS8(p + 1u));
    return m->call4(m->ctx, 0x800268BCu, actor, RD32(0x801022CCu + 4u * (idx + 29u)), actor, 0x800FFACCu);
}
