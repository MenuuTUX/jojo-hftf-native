#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800F9514(const GuestMem *m, uint32_t a0) {
    uint32_t s1 = RD32(a0 + 0x170u);
    uint32_t v1 = RD8(a0 + 7u);
    uint32_t t;

    if (v1 == 0) {
        WR8(a0 + 7u, RD8(a0 + 7u) + 1);
        WR16(a0 + 0xCu, 14);
        WR16(a0 + 0xEu, 0);
        WR32(a0 + 0x108u, 0);
        m->call4(m->ctx, 0x800BB2A4u, a0, 32, 0, 0);
        m->call4(m->ctx, 0x800BB30Cu, a0, 0, 0, 0);
        return m->call4(m->ctx, 0x800268BCu, a0, RD32(0x80102244u), 0, 0);
    }

    if (v1 == 1) {
        m->call4(m->ctx, 0x80026A04u, a0, 0, 0, 0);
        t = (uint32_t)(int32_t)(int16_t)(RD16(a0 + 0xCu) - 1);
        WR16(a0 + 0xCu, t);
        if ((int32_t)t != -1) return t;
        m->call4(m->ctx, 0x80024104u, a0, 282, 0, 0);
        v1 = RD8(a0 + 7u);
        WR16(a0 + 0xCu, 4);
        WR16(a0 + 0xEu, 0);
        v1 = v1 + 1u;
        WR8(a0 + 7u, v1);
        WR8(s1 + 0x1FBu, 2);
        v1 = RD8(a0 + 0xBu);
        WR8(a0 + 0x1FBu, 2);
        WR32(a0 + 0x70u, 0x70000u);
        WR32(a0 + 0x74u, 0xFFFA0000u);
        WR32(a0 + 0x78u, 0x4000u);
        WR32(a0 + 0x7Cu, 0);
        if (v1 != 0) return 0x4000u;
        WR32(a0 + 0x70u, 0xFFF90000u);
        WR32(a0 + 0x78u, 0xFFFFC000u);
        return 0xFFFFC000u;
    }

    if (v1 == 2) {
        if (m->call4(m->ctx, 0x800B2738u, a0, 0, 0, 0) != 0) {
            WR16(a0 + 0x6Eu, 35);
            WR8(a0 + 7u, 4);
            return m->call4(m->ctx, 0x800268BCu, a0, RD32(0x801020E0u), 0, 0);
        }
        t = (uint32_t)(int32_t)(int16_t)(RD16(a0 + 0xCu) - 1);
        WR16(a0 + 0xCu, t);
        if ((int32_t)t == -1) {
            WR16(a0 + 0xCu, 4);
            m->call4(m->ctx, 0x800FB7E8u, a0, 3, 2, 0);
            m->call4(m->ctx, 0x800FB7E8u, a0, 3, 3, 0);
            m->call4(m->ctx, 0x800FB7E8u, a0, 3, 4, 0);
        }
        m->call4(m->ctx, 0x80026A04u, a0, 0, 0, 0);
        m->call4(m->ctx, 0x800B428Cu, a0, 0, 0, 0);
        if (RD8(a0 + 0x139u) == 0) return 0x50000u;
        WR32(a0 + 0x70u, 0x50000u);
        WR32(a0 + 0x78u, 0xFFFFC000u);
        WR32(a0 + 0x74u, 0xFFFF0000u);
        WR32(a0 + 0x7Cu, 0xFFFF8000u);
        v1 = RD8(a0 + 7u) + 1u;
        t = RD8(a0 + 0xBu);
        WR8(a0 + 7u, v1);
        if (t != 0) {
            WR32(a0 + 0x70u, 0xFFFB0000u);
            WR32(a0 + 0x78u, 0x4000u);
        }
        return m->call4(m->ctx, 0x800268BCu, a0, RD32(0x80102274u), 0, 0);
    }

    if (v1 == 3) {
        m->call4(m->ctx, 0x80026A04u, a0, 0, 0, 0);
        m->call4(m->ctx, 0x800B428Cu, a0, 0, 0, 0);
        if (m->call4(m->ctx, 0x800B2738u, a0, 0, 0, 0) == 0) return 35;
        WR16(a0 + 0x6Eu, 35);
        WR16(a0 + 0xCu, 4);
        WR8(a0 + 7u, 4);
        return m->call4(m->ctx, 0x800268BCu, a0, RD32(0x8010227Cu), 0, 0);
    }

    m->call4(m->ctx, 0x80026A04u, a0, 0, 0, 0);
    if (RDS8(a0 + 0x4Du) != -1) return 0xFFFFFFFFu;
    t = (uint32_t)(int32_t)(int16_t)(RD16(a0 + 0xCu) - 1);
    WR16(a0 + 0xCu, t);
    if ((int32_t)t != -1) return t;
    return m->call4(m->ctx, 0x800C1F38u, a0, 0, 0, 0);
}
