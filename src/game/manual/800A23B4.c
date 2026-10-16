#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800A23B4(const GuestMem *m, uint32_t a0, uint32_t a1) {
    uint32_t p = RD32(0x800A34E4u);         /* lw a2,0x34e4(a3) with a3 = 0x800A0000 */
    uint32_t s;
    uint32_t v0;
    uint32_t v1;
    uint32_t tbl;
    uint32_t val;
    uint32_t hw;
    uint32_t a2v;
    uint32_t a3v;

    if (RD8(p + 5u) == 0u) {
        /* 800A23D0..800A2438 */
        WR8(p + 5u, RD8(p + 5u) + 1u);
        WR8(a0 + 0u, 1u);
        WR32(a0 + 4u, 0x801F1800u);
        WR16(a0 + 12u, 384u);
        tbl = RD32(p + 8u);
        v1 = RD32(tbl + ((a1 & 0xFFu) << 2));
        WR16(a0 + 16u, 32u);
        WR16(a0 + 18u, 70u);
        WR16(a0 + 14u, 4u);
        WR8(a0 + 2u, 24u);
        WR8(a0 + 3u, 16u);
        WR32(a0 + 8u, v1);
        /* jal 800309D0: a0, a1 unchanged; a2 = p and a3 = 0x800A0000 are set by this function */
        v0 = m->call4(m->ctx, 0x800309D0u, a0, a1, p, 0x800A0000u);
        WR32(0x800F89B8u, v0);
        return v0;
    }

    /* 800A244C..800A24A0 */
    tbl = RD32(p + 8u);
    val = RD32(tbl + ((a1 & 0xFFu) << 2));
    s = RD32(0x800F89B8u);
    WR8(s + 11u, 0u);
    WR32(s + 64u, val);

    hw = RD16(a0 + 14u);                    /* lhu a0,0xe(t0): t0 is the original a0 */
    s = RD32(0x800F89B8u);
    a2v = RD8(s + 178u);
    a3v = RD8(s + 179u);
    v1 = RD16(s + 30u);
    WR16(s + 14u, hw);
    WR16(s + 12u, hw);

    return m->call5(m->ctx, 0x80030784u, RD32(s + 60u), RD32(s + 64u), a2v, a3v, v1);
}
