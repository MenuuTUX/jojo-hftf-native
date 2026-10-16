#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FB54C(const GuestMem *m, uint32_t a0) {
    uint32_t t;
    if (RD8(a0 + 8u) == 0) {
        WR8(a0 + 8u, 1);
        WR16(a0 + 0xCu, 70);
        m->call4(m->ctx, 0x800268BCu, a0, RD32(0x801023D8u), 0, 0);
    }
    m->call4(m->ctx, 0x80026A04u, a0, 0, 0, 0);
    if (RDS8(a0 + 0x4Du) != 0) m->call4(m->ctx, 0x800FB7E8u, a0, 5, 0, 0);
    t = (uint32_t)(int32_t)(int16_t)(RD16(a0 + 0xCu) - 1);
    WR16(a0 + 0xCu, t);
    if ((int32_t)t == -1) return m->call4(m->ctx, 0x800C1F38u, a0, 0, 0, 0);
    return t;
}
