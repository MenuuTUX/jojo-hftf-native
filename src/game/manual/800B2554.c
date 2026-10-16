#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800B2554(const GuestMem *m, uint32_t a0) {
    uint32_t s0 = a0;
    uint32_t s1 = RD32(s0 + 368);
    uint32_t v2 = RD32(s0 + 264);
    uint32_t t, v0;

    WR8(s0 + 521, 1);
    if (v2 == 0)
        return m->call4(m->ctx, 0x800AF7ECu, s0, 0, 0, 0);
    if (RD8(s0 + 507) != 0) {
        if (RD8(s0 + 507) == 1)
            m->call4(m->ctx, 0x800BB708u, s0, 0, 0, 0);
        return m->call4(m->ctx, 0x80026A04u, s0, 0, 0, 0);
    }
    if (RD8(s0 + 7) != 0) {
        if (RD8(v2 + 11) != 0) {
            if (RDS16(s0 + 106) < RDS16(v2 + 106))
                return m->call4(m->ctx, 0x800AF7ECu, s0, 0, 0, 0);
        } else if (RDS16(v2 + 106) < RDS16(s0 + 106)) {
            return m->call4(m->ctx, 0x800AF7ECu, s0, 0, 0, 0);
        }
        t = RD16(s0 + 12) - 1u;
        WR16(s0 + 12, t);
        if ((t << 16) == 0)
            return m->call4(m->ctx, 0x800AF7ECu, s0, 0, 0, 0);
        return m->call4(m->ctx, 0x80026A04u, s0, 0, 0, 0);
    }
    WR8(s0 + 7, RD8(s0 + 7) + 1);
    WR16(s0 + 12, 16);
    m->call4(m->ctx, 0x800B4FCCu, s0, 0, 0, 0);
    m->call4(m->ctx, 0x800C6FF8u, s0, 0, 0, 0);
    m->call4(m->ctx, 0x80024104u, s0, 107, 0, 0);
    m->call4(m->ctx, 0x80024104u, s0, 263, 0, 0);
    if (RD8(s0 + 508) != 0) {
        v0 = RD16(s0 + 460) & 0x4000u;
        if (v0 != 0)
            return v0;
        WR8(s0 + 508, 0);
        m->call4(m->ctx, 0x800B4AD0u, s0, 27, 0, 0);
        v0 = RD8(RD32(s0 + 304) + 52);
        if (v0 != 0)
            return v0;
        WR8(s1 + 508, 0);
        return m->call4(m->ctx, 0x800B4AD0u, s1, 27, 0, 0);
    }
    v0 = RD16(s0 + 460) & 0x4000u;
    if (v0 == 0)
        return v0;
    WR8(s0 + 508, 1);
    m->call4(m->ctx, 0x800B4AD0u, s0, 30, 0, 0);
    v0 = RD8(RD32(s0 + 304) + 52);
    if (v0 != 0)
        return v0;
    WR8(s1 + 508, 1);
    return m->call4(m->ctx, 0x800B4AD0u, s1, 30, 0, 0);
}
