#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FAAD8(const GuestMem *m, uint32_t a0) {
    uint32_t s1 = RD32(a0 + 0x130u);
    uint32_t v5, t, v;

    if (RD8(a0 + 7u) == 0) {
        m->call4(m->ctx, 0x800BB2A4u, a0, 71, 0, 0);
        WR8(a0 + 0x204u, 1);
        WR8(a0 + 7u, RD8(a0 + 7u) + 1);
        m->call4(m->ctx, 0x80024104u, a0, 286, 0, 0);
        m->call4(m->ctx, 0x80024104u, a0, 289, 0, 0);
        if (RD8(a0 + 0xBu) != 0) {
            WR32(a0 + 0x70u, 0x100000u);
            WR32(a0 + 0x78u, 0xFFFF0000u);
        } else {
            WR32(a0 + 0x70u, 0xFFF00000u);
            WR32(a0 + 0x78u, 0x10000u);
        }
        m->call4(m->ctx, 0x800FB8A0u, a0, 0, 0, 0);
        WR16(a0 + 0xCu, 8);
        WR16(a0 + 0xEu, 1);
        m->call4(m->ctx, 0x800268BCu, a0, RD32(0x80102294u), 0, 0);
    }

    v5 = RD8(a0 + 7u);
    if (v5 == 1) {
        m->call4(m->ctx, 0x80026A04u, a0, 0, 0, 0);
        m->call4(m->ctx, 0x800B42D0u, a0, 0, 0, 0);
        t = (uint32_t)(int32_t)(int16_t)(RD16(a0 + 0xEu) - 1);
        WR16(a0 + 0xEu, t);
        if ((int32_t)t == -1) {
            WR16(a0 + 0xEu, 2);
            m->call4(m->ctx, 0x800C9680u, a0, 12, RD8(a0 + 0x32u) + 1u, 0);
        }
        t = (uint32_t)(int32_t)(int16_t)(RD16(a0 + 0xCu) - 1);
        WR16(a0 + 0xCu, t);
        if ((int32_t)t == -1) {
            WR16(a0 + 0xCu, 2);
            v = RD8(a0 + 7u) + 1u;
            WR8(a0 + 7u, v);
            return v;
        }
        return t;
    }

    if (v5 == 2) {
        t = (uint32_t)(int32_t)(int16_t)(RD16(a0 + 0xCu) - 1);
        WR16(a0 + 0xCu, t);
        if ((int32_t)t != -1) return t;
        WR8(a0 + 7u, RD8(a0 + 7u) + 1);
        m->call4(m->ctx, 0x80024104u, a0, 210, 0, 0);
        m->call4(m->ctx, 0x800BB2A4u, a0, 72, 0, 0);
        m->call4(m->ctx, 0x800FB904u, a0, 0, 0, 0);
        m->call4(m->ctx, 0x800C7374u, a0, 0, 0, 0);
        m->call4(m->ctx, 0x8002B450u, 5, 2, 0, 0);
        if (RD8(a0 + 0xBu) != 0) {
            WR32(a0 + 0x70u, 0x20000u);
            v = 0xFFFFC000u;
        } else {
            WR32(a0 + 0x70u, 0xFFFE0000u);
            v = 0x4000u;
        }
        WR32(a0 + 0x78u, v);
        return m->call4(m->ctx, 0x800268BCu, a0, RD32(0x80102298u), 0, 0);
    }

    if (v5 == 3) {
        m->call4(m->ctx, 0x80026A04u, a0, 0, 0, 0);
        if (RD32(a0 + 0x70u) != 0) m->call4(m->ctx, 0x800B42D0u, a0, 0, 0, 0);
        if (RDS8(a0 + 0x4Du) == -1) {
            WR8(s1 + 0x20u, 0);
            return m->call4(m->ctx, 0x800C1F38u, a0, 0, 0, 0);
        }
        return 0xFFFFFFFFu;
    }

    return 3;
}
