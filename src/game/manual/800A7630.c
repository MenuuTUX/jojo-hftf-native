#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800A7630(const GuestMem *m) {
    const uint32_t s0 = 0x8006D2D0u;
    const uint32_t s1 = 0x80066CA0u;
    int32_t mode;
    int32_t sv;
    uint32_t sh;
    uint32_t d;
    uint32_t r1;
    uint32_t r2;

    /* 800A7658..800A7664: byte 0x80066C6C (signed) == 3 returns with $v0 = 0x80070000 left by the lui at 800A7650 */
    mode = RDS8(0x80066C6Cu);
    if (mode == 3) {
        return 0x80070000u;
    }
    /* 800A766C..800A7688: unless mode is 5 or 0, byte 0x8006CE51 == 3 returns it (as $v0) */
    if (mode != 5 && mode != 0) {
        sv = RDS8(0x8006CE51u);
        if (sv == 3) {
            return (uint32_t)sv;
        }
    }

    /* 800A7690..800A76AC: byte +7 == 0 returns 0; byte +9 and byte +2 return themselves when nonzero */
    if (RD8(s1 + 7u) == 0u) {
        return 0u;
    }
    if (RD8(s1 + 9u) != 0u) {
        return RD8(s1 + 9u);
    }
    if (RD8(s1 + 2u) != 0u) {
        return RD8(s1 + 2u);
    }

    /* 800A76C0..800A76E4: byte +0x11 of s0 decremented; the shifted value is returned in the cases the disasm shows */
    d = (uint32_t)RD8(s0 + 0x11u) - 1u;
    WR8(s0 + 0x11u, d);
    sh = d << 24;
    if (RDS8(s1 + 0x24u) != 0) {
        if ((int32_t)sh >= 0) {
            return sh;
        }
    } else if (sh != 0u) {
        return sh;
    }

    /* 800A76F4..800A7708: byte +0x11 set to 99 when byte +0x24 is nonzero, else 70 */
    WR8(s0 + 0x11u, (RDS8(s1 + 0x24u) != 0) ? 99u : 70u);

    /* 800A770C..800A771C: byte +0x10 decremented; returns $v0 = 0x800D0000 (lui in the delay slot) when non-negative */
    d = (uint32_t)RD8(s0 + 0x10u) - 1u;
    WR8(s0 + 0x10u, d);
    if ((int32_t)(d << 24) >= 0) {
        return 0x800D0000u;
    }

    /* 800A7728..800A7738: both bytes cleared, then 800A77EC on the table at 0x800CDD50 */
    WR8(s0 + 0x11u, 0u);
    WR8(s0 + 0x10u, 0u);
    r1 = m->call4(m->ctx, 0x800A77ECu, 0x800CDD50u, 0u, 0u, 0u);
    if (r1 != 0u) {
        WR8(s0 + 0x11u, 1u);
        return 1u;
    }

    /* 800A7744..800A7750: second call on 0x800CE0E0; zero result sets bytes +0xE and +9 of s1 */
    r2 = m->call4(m->ctx, 0x800A77ECu, 0x800CE0E0u, 0u, 0u, 0u);
    if (r2 == 0u) {
        WR8(s1 + 0xEu, 1u);
        WR8(s1 + 9u, 1u);
        return 1u;
    }
    WR8(s0 + 0x11u, 1u);
    return 1u;
}
