#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800F7428(const GuestMem *m, uint32_t a0) {
    uint32_t s1 = RD32(a0 + 0x170u);
    int32_t v3;
    uint32_t v0;
    m->call4(m->ctx, 0x80026A04u, a0, 0, 0, 0);
    v3 = 35;
    if (RD8(a0 + 0x1FBu) == 1) {
        uint16_t h = RD16(s1 + 0x6Eu);
        WR16(a0 + 0x6Eu, h);
        v0 = (uint32_t)(int32_t)(int16_t)h;
    } else {
        v3 = RDS8(a0 + 0x4Du);
        v0 = 0xFFFFFFFFu;
    }
    if (v3 == (int32_t)v0) return m->call4(m->ctx, 0x800AF7ECu, a0, 0, 0, 0);
    return v0;
}
