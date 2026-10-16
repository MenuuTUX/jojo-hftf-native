#include "manual.h"
#include "hex2c_rt.h"

/* 800C9FE4: $v0 is read at entry (beq v0,zero at 800C9FE8). in_v0 == 0 advances byte +5 and returns it + 1.
 * Otherwise it steps the position at +0x70 by its velocity +0x78, after the per-frame updates below. The v3 < 0 case
 * clears byte +6 before calling 800C94A0(a0) and returns that call's $v0 (800C94A0 reads a0 before writing; the other
 * argument registers are passed as 0). */
uint32_t man_800C9FE4(const GuestMem *m, uint32_t a0, uint32_t in_v0) {
    uint32_t v;
    uint32_t d;
    int32_t v3;
    uint32_t r;

    if (in_v0 == 0u) {
        v = RD8(a0 + 5u) + 1u;
        WR8(a0 + 5u, v);
        return v;
    }
    if (RD8(a0 + 6u) != 0u) {
        d = RD16(a0 + 218u) - RD16(a0 + 112u);
        WR16(a0 + 218u, d);
        WR16(a0 + 216u, d);
        v3 = RDS16(a0 + 216u);
        WR16(a0 + 238u, RD16(a0 + 238u) - 8u);
        if (v3 < 0) {
            WR8(a0 + 6u, 0u);
            return m->call4(m->ctx, 0x800C94A0u, a0, 0u, 0u, 0u);
        }
    } else {
        d = RD16(a0 + 220u) - RD16(a0 + 112u);
        WR16(a0 + 220u, d);
        if ((int16_t)d < 0) {
            WR16(a0 + 220u, 0u);
            WR8(a0 + 6u, 1u);
        }
    }
    r = RD32(a0 + 112u) + RD32(a0 + 120u);
    WR32(a0 + 112u, r);
    return r;
}
