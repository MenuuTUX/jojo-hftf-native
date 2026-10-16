#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800C70B0(const GuestMem *m, uint32_t a0);

uint32_t man_800B28CC(const GuestMem *m, uint32_t a0) {
    uint32_t s1 = a0;
    uint32_t s0 = RD32(s1 + 368);
    uint32_t a2v = RD32(s1 + 380);
    uint32_t b7, t, v1, v5, v6;
    int32_t t311;

    WR8(s1 + 514, 1);
    WR8(s0 + 514, 1);
    WR16(s0 + 460, RD16(s1 + 460));
    WR16(s0 + 462, RD16(s1 + 462));
    WR16(s0 + 470, RD16(s1 + 470));
    b7 = RD8(s1 + 7);

    if (b7 == 0) {
        WR8(s1 + 7, b7 + 1);
        WR8(s1 + 563, 40);
        WR8(s1 + 263, 30);
        m->call4(m->ctx, 0x800BB27Cu, s1, 0, 0, 0);
        m->call4(m->ctx, 0x800BB27Cu, s0, 0, 0, 0);
        WR8(s1 + 508, 0);
        WR8(s0 + 508, 0);
        WR16(s0 + 590, 0);
        WR16(s0 + 592, 0);
        WR8(s0 + 506, 0);
        v5 = RD8(s1 + 502);
        WR8(s1 + 11, v5);
        v6 = RD16(s0 + 132);
        WR8(s0 + 502, v5);
        WR8(s0 + 11, v5);
        WR16(s0 + 110, 35);
        WR16(s0 + 240, 63);
        WR16(s0 + 238, 63);
        WR8(s0 + 224, 0x80);
        WR8(s0 + 228, 0x80);
        WR8(s0 + 232, 0x80);
        WR8(s0 + 318, 0);
        WR8(s0 + 319, 0);
        WR8(s0 + 184, 0);
        WR8(s0 + 604, 0);
        WR16(s0 + 30, v6);
        WR8((uint32_t)RDS8(s1 + 310) + 0x800CDD48u, 0);
        WR8((uint32_t)RDS8(s1 + 310) + 0x800CDD4Au, 0);
        m->call4(m->ctx, 0x80024104u, s1, 264, 0, 0);
        if (RD16(s1 + 110) == 35)
            return m->call4(m->ctx, 0x800B4AD0u, s1, 40, 0, 0);
        return m->call4(m->ctx, 0x800B4AD0u, s1, 21, 0, 0);
    }

    if (b7 == 1) {
        t311 = RDS8(s1 + 311);
        if (t311 != 0 || RDS8(s1 + 77) == -1) {
            WR8(s1 + 7, b7 + 1);
            m->call4(m->ctx, 0x800BC074u, s1, 0xFFFFFFF9u, 80, 0);
            man_800C70B0(m, s1);
        }
        return m->call4(m->ctx, 0x80026A04u, s1, 0, 0, 0);
    }

    if (b7 == 2) {
        t311 = RDS8(s1 + 311);
        if (t311 == 4 || t311 == 6 || t311 == 7) {
            WR8(s1 + 7, 4);
            return 4;
        }
        if (RD8(0x80066CB0u) == 0) {
            WR8(s1 + 7, b7 + 1);
            return b7 + 1;
        }
        v1 = RD16(s1 + 470);
        if ((v1 & 0x80) != 0)
            m->call4(m->ctx, 0x80024880u, s1, 0, 0, 0);
        else if ((v1 & 0x10) != 0)
            m->call4(m->ctx, 0x80024880u, s1, 1, 0, 0);
        else if ((v1 & 0x20) != 0)
            m->call4(m->ctx, 0x80024880u, s1, 2, 0, 0);
        if (m->calli(m->ctx, RD32(RD32(s0 + 300) + 28), s0, 0, 0, 0) != 0)
            return m->call4(m->ctx, 0x800B2F0Cu, s1, 0, 0, 0);
        if (m->call4(m->ctx, 0x800C36F4u, s0, 0, 0, 0))
            return m->call4(m->ctx, 0x800B2E14u, s1, 0, 0, 0);
        return m->call4(m->ctx, 0x80026A04u, s1, 0, 0, 0);
    }

    if (b7 == 3) {
        if (m->call4(m->ctx, 0x800B2D50u, s1, 0, 0, 0) == 0 || (RD16(s1 + 460) & 0x40) == 0) {
            t = (uint32_t)RD8(s1 + 7) + 1u;
            WR8(s1 + 7, t);
            return t;
        }
        v1 = RD16(s1 + 470);
        if ((v1 & 0x80) != 0)
            m->call4(m->ctx, 0x80024880u, s1, 0, 0, 0);
        else if ((v1 & 0x10) != 0)
            m->call4(m->ctx, 0x80024880u, s1, 1, 0, 0);
        else if ((v1 & 0x20) != 0)
            m->call4(m->ctx, 0x80024880u, s1, 2, 0, 0);
        if (m->calli(m->ctx, RD32(RD32(s0 + 300) + 28), s0, 0, 0, 0) != 0)
            return m->call4(m->ctx, 0x800B2F0Cu, s1, 0, 0, 0);
        if (m->call4(m->ctx, 0x800C36F4u, s0, 0, 0, 0))
            m->call4(m->ctx, 0x800B2E14u, s1, 0, 0, 0);
        return m->call4(m->ctx, 0x80026A04u, s1, 0, 0, 0);
    }

    /* default: b7 not 0..3 */
    WR16(s0 + 238, 63);
    WR16(s0 + 240, 63);
    WR8(s1 + 263, 16);
    WR8(s0 + 263, 16);
    WR8(s1 + 515, 1);
    WR8(s0 + 515, 1);
    WR8(s0 + 1, 1);
    WR8(s0 + 0, 1);
    WR8(s1 + 517, 1);
    WR8(s0 + 506, 22);
    WR8(s0 + 517, 1);
    WR8(s0 + 5, 1);
    WR8(s0 + 6, 0);
    WR8(s0 + 7, 0);
    WR8(s0 + 8, 0);
    WR8(s0 + 10, 0);
    WR8(s0 + 9, 0);
    WR8((uint32_t)RDS8(s1 + 310) + 0x800CDD4Au, 0);
    t311 = RDS8(s1 + 311);
    if (t311 == 4 || t311 == 6 || t311 == 7) {
        WR8(s1 + 1, 0);
        WR16(s0 + 106, RD16(s1 + 106));
        WR16(s0 + 110, RD16(s1 + 110));
        WR8(s0 + 11, RD8(s1 + 11));
        WR8(s0 + 507, 1);
        WR8(s0 + 502, RD8(s1 + 502));
        WR8(s1 + 507, 1);
        m->call4(m->ctx, 0x800B4AD0u, s0, 40, 0, 0);
        return m->call4(m->ctx, 0x800AF7ECu, s1, 0, 0, 0);
    }
    if (t311 == 8) {
        if (RDS16(a2v + 106) < RDS16(s0 + 106)) {
            WR8(s0 + 11, 0);
            WR8(s0 + 502, 0);
        } else {
            WR8(s0 + 11, 1);
            WR8(s0 + 502, 1);
        }
        WR16(s0 + 110, 35);
        WR8(s0 + 507, 0);
    } else {
        WR16(s0 + 110, 35);
        WR16(s0 + 106, RD16(s1 + 106));
        WR8(s0 + 11, RD8(s1 + 11));
        t = RD8(s1 + 502);
        WR8(s0 + 507, 0);
        WR8(s0 + 502, t);
    }
    WR8(s1 + 507, 0);
    return m->call4(m->ctx, 0x800AF7ECu, s1, 0, 0, 0);
}
