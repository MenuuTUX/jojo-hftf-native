#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FD890(const GuestMem *m, uint32_t a0, uint32_t in_v0) {
    uint32_t s0 = a0;
    uint32_t v;

    /* 800FD89C: bne v0,zero,800FD910 returns with $v0 unchanged */
    if (in_v0 != 0u) {
        return in_v0;
    }
    /* 800FD8A4..800FD8E8: byte+6 nonzero: jal 80026A04, then the same -1 test as 800FD7E0 */
    if (RD8(s0 + 6u) != 0u) {
        m->call4(m->ctx, 0x80026A04u, s0, 0u, 0u, 0u);
        if (RDS8(s0 + 0x4Du) != -1) {
            return 0xFFFFFFFFu;
        }
        v = RD8(s0 + 5u) + 1u;
        WR8(s0 + 5u, v);
        return v;
    }
    /* 800FD8B4..800FD8C8: decrement the halfword at +0xC (stored low 16 bits); the value returned is its sign extension */
    {
        uint16_t cnt = (uint16_t)(RD16(s0 + 0xCu) - 1u);
        int32_t sv = (int16_t)cnt;
        WR16(s0 + 0xCu, cnt);
        if (sv != -1) {
            return (uint32_t)sv;
        }
        /* 800FD8D4..800FD8E0: byte+1 = 1, byte+6 incremented, the new byte+6 is returned */
        WR8(s0 + 1u, 1u);
        v = RD8(s0 + 6u) + 1u;
        WR8(s0 + 6u, v);
        return v;
    }
}
