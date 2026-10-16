#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FDB9C(const GuestMem *m, uint32_t a0, uint32_t in_v0) {
    uint32_t s0 = a0;
    uint32_t c;
    uint32_t v;

    /* 800FDBA8: bne v0,zero,800FDC18 returns with $v0 unchanged */
    if (in_v0 != 0u) {
        return in_v0;
    }
    if (RD8(s0 + 6u) == 0u) {
        /* 800FDBC0..800FDBD4: byte+1 = 0, halfword at +0xC decremented and stored; returns (count << 16) when nonzero */
        c = RD16(s0 + 0xCu) - 1u;
        WR8(s0 + 1u, 0u);
        WR16(s0 + 0xCu, c);
        if ((c << 16) != 0u) {
            return c << 16;
        }
        /* 800FDBD8..800FDBE8: byte+1 = 1, byte+6 incremented; the new byte+6 is returned */
        WR8(s0 + 1u, 1u);
        v = RD8(s0 + 6u) + 1u;
        WR8(s0 + 6u, v);
        return v;
    }
    /* 800FDBF0..800FDC00: jal 80026A04 with $a0 = s0, then the same -1 test as 800FD7E0 */
    m->call4(m->ctx, 0x80026A04u, s0, 0u, 0u, 0u);
    if (RDS8(s0 + 0x4Du) != -1) {
        return 0xFFFFFFFFu;
    }
    v = RD8(s0 + 5u) + 1u;
    WR8(s0 + 5u, v);
    return v;
}
