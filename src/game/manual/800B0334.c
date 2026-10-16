#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800B0334(const GuestMem *m, uint32_t a0) {
    uint32_t s0 = a0;
    uint32_t s1 = RD32(s0 + 368);
    uint32_t s2 = 0;   /* $s2 is only set for $539 in 0..3; other values leave the caller's $s2 (not modelled) */
    uint32_t b504, b539, a1v;

    m->call4(m->ctx, 0x800BC608u, s0, 0x20000000u, 0, 0);
    if (RDS8(0x80066C6Cu) == 3)
        m->call4(m->ctx, 0x800BC8A8u, s0, 0, 0, 0);
    WR8(s0 + 506, 4);
    WR8(s0 + 6, 0);
    WR8(s0 + 10, 0);
    WR8(s0 + 9, 0);
    WR8(s0 + 8, 0);
    WR8(s0 + 7, 0);
    if (RD8(s0 + 314) == 0 && RD8(s0 + 507) == 1) {
        WR8(s0 + 539, RD8(s1 + 539));
        WR8(s0 + 504, RD8(s1 + 504));
        WR8(s0 + 11, RD8(s1 + 11));
        WR8(s0 + 502, RD8(s1 + 502));
        WR32(s0 + 116, RD32(s1 + 116));
        WR32(s0 + 124, RD32(s1 + 124));
        WR32(s0 + 112, RD32(s1 + 112));
        WR32(s0 + 120, RD32(s1 + 120));
        WR16(s0 + 108, RD16(s1 + 108));
        m->call4(m->ctx, 0x800BB708u, s0, 0, 0, 0);
        goto L25;
    }
    WR16(s0 + 12, 6);
    WR8(s0 + 11, RD8(s0 + 502));
    if (RD8(s0 + 509) == 0)
        m->call4(m->ctx, 0x800AFD84u, s0, 0, 0, 0);

    b539 = RD8(s0 + 539);
    if (b539 == 0 || b539 == 2) {
        s2 = RD32(RD32(s0 + 300) + 16);
        if (RD8(s0 + 513) != 0)
            s2 += 16;
    } else if (b539 == 1) {
        s2 = RD32(RD32(s0 + 300) + 16) + 8;
    } else if (b539 == 3) {
        s2 = RD32(RD32(s0 + 300) + 16) + 32;
    }
    WR8(s1 + 612, 0);
    WR8(s0 + 612, 0);
    WR32(s0 + 116, (uint32_t)RDS16(s2 + 4) << 8);
    b504 = RD8(s0 + 504);
    WR32(s0 + 120, 0);
    WR32(s0 + 112, 0);
    WR32(s0 + 124, (uint32_t)RDS16(s2 + 6) << 8);
    if (b504 == 0) {
        WR32(s0 + 112, (uint32_t)RDS16(s2 + 0) << 8);
    } else if (b504 == 1) {
        WR32(s0 + 112, 0u - ((uint32_t)RDS16(s2 + 2) << 8));
    }
    if (RD8(s0 + 11) != 0) {
        uint32_t t70 = RD32(s0 + 112);
        uint32_t t78 = RD32(s0 + 120);
        WR32(s0 + 112, 0u - t70);
        WR32(s0 + 120, 0u - t78);
    }

L25:
    if (RD8(s0 + 513) != 0)
        return m->call4(m->ctx, 0x800B4AD0u, s0, 3, 0, 0);
    if (RDS8(s0 + 311) == 17 && RD8(s0 + 507) != 0 && RD8(s0 + 539) == 2)
        return 2;
    b504 = RD8(s0 + 504);
    if (b504 == 0)
        a1v = 12;
    else if (b504 == 1)
        a1v = 14;
    else
        a1v = 10;
    return m->call4(m->ctx, 0x800B4AD0u, s0, a1v, 0, 0);
}
