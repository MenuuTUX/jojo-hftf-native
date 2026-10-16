#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800CA580(const GuestMem *m, uint32_t a0) {
    uint32_t v;
    uint32_t r;

    v = (RD16(a0 + 12u) - 1u) & 0xFFFFu;
    WR16(a0 + 12u, v);
    r = (uint32_t)(int32_t)(int16_t)v;
    if ((int32_t)r == -1) {
        m->call4(m->ctx, 0x80025724u, a0, 0u, 0u, 0u);
        v = RD8(0x80066C74u) - 1u;
        WR8(0x80066C74u, v);
        return v;
    }
    return r;
}
