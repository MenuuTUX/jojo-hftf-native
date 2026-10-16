#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_8001C6B4(const GuestMem *m) {
    uint32_t s0 = 0x800797B0u;

    m->call4(m->ctx, 0x8001ED60u, 0x207u, 0u, 0u, 0u);

    WR16(s0 + 0u, 0u);
    WR16(s0 + 2u, 510u);
    WR16(s0 + 4u, 384u);
    WR16(s0 + 6u, 2u);

    /* LoadImage(s0, 0x80115800); a2/a3 left by the earlier callee are not modelled (passed as 0) */
    m->call4(m->ctx, 0x80036348u, s0, 0x80115800u, 0u, 0u);

    /* 80027414(2, s0, 0x80115800); its v0 is the return value */
    return m->call4(m->ctx, 0x80027414u, 2u, s0, 0x80115800u, 0u);
}
