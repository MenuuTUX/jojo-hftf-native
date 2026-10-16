#include "manual.h"
#include "hex2c_rt.h"

/* 800CAD70: state byte +6 selects the branch. Returns the value left in $v0 at jr ra on each path (see the json). */
uint32_t man_800CAD70(const GuestMem *m, uint32_t a0) {
    uint32_t t;
    uint32_t r;

    if (RD8(a0 + 3u) != 0u) {
        if (RDS8(0x80066CC0u) == 0) {
            WR8(a0 + 5u, RD8(a0 + 5u) + 1u);
        }
    } else {
        if (RD8(0x80066CB0u) == 0u) {
            WR8(a0 + 1u, 0u);
            WR8(a0 + 5u, RD8(a0 + 5u) + 1u);
        }
    }

    if (RD8(a0 + 6u) == 0u) {
        if (RD8(a0 + 11u) != 0u) {
            if (RDS16(a0 + 106u) >= 385) {
                r = RD32(a0 + 104u) - 0x100000u;
                WR32(a0 + 104u, r);
                return r;
            }
        } else if (RDS16(a0 + 106u) < -2) {
            r = RD32(a0 + 104u) + 0x100000u;
            WR32(a0 + 104u, r);
            return r;
        }
        WR8(a0 + 6u, 1u);
        return 1u;
    }

    if (RD8(a0 + 6u) == 1u && RD8(a0 + 3u) != 0u) {
        if (RD16(a0 + 12u) != 0u) {
            r = RD16(a0 + 12u) - 1u;
            WR16(a0 + 12u, r);
            return r;
        }
        WR8(a0 + 6u, 2u);
        return 2u;
    }

    if (RD8(a0 + 6u) != 2u) {
        return 2u;
    }
    if (RD8(a0 + 3u) == 0u) {
        return 0u;
    }
    if (RD8(a0 + 11u) != 0u) {
        t = m->call4(m->ctx, 0x80025BF8u, RD8(a0 + 6u), 0u, 0u, 0u) & 0xFu;
    } else {
        t = 0u - (m->call4(m->ctx, 0x80025BF8u, RD8(a0 + 6u), 0u, 0u, 0u) & 0xFu);
    }
    WR16(a0 + 180u, t);
    r = (m->call4(m->ctx, 0x80025BF8u, 0u, 0u, 0u, 0u) & 0xFu) - 8u;
    WR16(a0 + 182u, r);
    return r;
}
