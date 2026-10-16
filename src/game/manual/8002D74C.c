#include "manual.h"
#include "hex2c_rt.h"

/* First half: steps the byte at +0xC0 by 2 with a ping-pong at 96 / 192 and flips the flag at +4
 * on the turn-around (the xori in the delay slots only sets $v0; the sb to +4 happens on the turn only). */
static void step_c0(const GuestMem *m, uint32_t a0) {
    uint32_t flag = RD8(a0 + 4u);
    int32_t v;

    if (flag != 0u) {
        v = (int32_t)RD8(a0 + 0xC0u) + 2;
        if (v >= 0xC1) {
            WR8(a0 + 4u, flag ^ 1u);
            WR8(a0 + 0xC0u, 0xC0u);
            return;
        }
    } else {
        v = (int32_t)RD8(a0 + 0xC0u) - 2;
        if (v < 96) {
            WR8(a0 + 4u, flag ^ 1u);
            WR8(a0 + 0xC0u, 96u);
            return;
        }
    }
    WR8(a0 + 0xC0u, (uint32_t)v);
}

/* 8002D74C (TEXT). Step of +0xC0 (see step_c0), then if the byte at 0x800959E9 is nonzero: +0x6E = table
 * 0x8005B078[idx] - 8, where idx is 0x800959EB when the byte at +3 is 1, else 0x800959EC (both signed).
 * Otherwise clears +0 and tail-calls 80025A50(a0, 0). $a2/$a3 pass through to 80025A50 unchanged. */
uint32_t man_8002D74C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    int32_t idx;
    uint32_t result;

    (void)a1;
    step_c0(m, a0);
    if (RDS8(0x800959E9u) != 0) {
        if (RD8(a0 + 3u) == 1u)
            idx = RDS8(0x800959EBu);
        else
            idx = RDS8(0x800959ECu);
        result = (uint32_t)RD16(0x8005B078u + ((uint32_t)idx << 1)) - 8u;
        WR16(a0 + 0x6Eu, result);
        return result;
    }
    WR8(a0, 0u);
    return m->call4(m->ctx, 0x80025A50u, a0, 0u, a2, a3);
}
