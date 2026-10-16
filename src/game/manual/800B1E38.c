#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800B1E38(const GuestMem *m, uint32_t a0) {
    uint32_t s0 = a0;
    uint32_t s1 = RD32(s0 + 368);
    uint32_t b507 = RD8(s0 + 507);
    uint32_t b52, v, r;
    int32_t t311;

    WR8(s0 + 506, 12);
    WR8(s0 + 6, 0);
    WR8(s0 + 8, 0);
    WR8(s0 + 7, 0);
    WR8(s0 + 10, 0);
    WR8(s0 + 9, 0);
    WR8(s0 + 521, 1);
    WR8(s0 + 335, 0);
    WR8(s0 + 523, 0);
    if (b507 == 0) {
        t311 = RDS8(s0 + 311);
        WR8(s0 + 546, 0);
        if (t311 == 6)
            m->calli(m->ctx, RD32(RD32(s0 + 300) + 100), s0, 0, 0, 0);
        m->call4(m->ctx, 0x800B1F6Cu, s0, 0, 0, 0);
        m->call4(m->ctx, 0x800B4B58u, s0, 27, 30, 0);
        b52 = RD8(RD32(s0 + 304) + 52);
        if (b52 != 0)
            return b52;
        WR8(s0 + 517, 1);
        return m->call4(m->ctx, 0x800C4778u, s1, 0, 0, 0);
    }
    WR8(s0 + 546, 1);
    if (b507 == 1) {
        v = RD8(s1 + 502);
        WR8(s0 + 11, v);
        WR8(s0 + 502, v);
        m->call4(m->ctx, 0x800BB708u, s0, 0, 0, 0);
    } else {
        r = m->call4(m->ctx, 0x800C25DCu, s1, 0xFFFFFFD0u, 64, 0);
        if (r == 0) {
            v = RD8(s1 + 502);
            WR8(s0 + 11, v);
            WR8(s0 + 502, v);
        }
    }
    return m->call4(m->ctx, 0x800B4B58u, s0, 49, 52, 0);
}
