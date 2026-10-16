#include "manual.h"
#include "hex2c_rt.h"

static uint32_t sext16(uint32_t x) { return (uint32_t)(int32_t)(int16_t)x; }

/* 8001729C (TEXT). $a0 = case number, $a1 = 16-bit argument, $a2 is overwritten (base) before any call, $a3 reaches the jal
 * to 80017448 unchanged, so nargs is 4.
 * The switch is the jump table at 0x800541A4 (one word per case 1..6, read from guest memory as the original `lw v0` does).
 * The block for case 3 does not write $v0, so it returns the table word itself, as the original `jr v0` leaves it.
 * Case 5 and 6 divide 28160 by the sign-extended $a1; the break 7 / break 6 checks never fire in the game, so a zero divisor
 * gives 0 here instead of a trap. The table holds only the six block addresses below (checked in the RAM image). */
uint32_t man_8001729C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const uint32_t base = 0x80079208u;
    uint32_t idx = a0 - 1u;
    uint32_t t;
    int32_t d, q;

    if (idx >= 6u)
        return 0u;

    t = RD32(0x800541A4u + idx * 4u);

    if (t == 0x800172D8u) {
        WR8(base + 0x2Eu, 0u);
        WR8(base + 0x2Cu, 0u);
        WR8(base + 0x31u, 0u);
        return m->call4(m->ctx, 0x80017448u, base, sext16(a1), base, a3);
    }
    if (t == 0x800172FCu) {
        WR8(base + 0x31u, 1u);
        return 1u;
    }
    if (t == 0x80017308u) {
        WR8(base + 0x31u, 0u);
        return t;
    }
    if (t == 0x80017310u) {
        WR8(base + 0x2Du, 5u);
        WR8(base + 0x2Eu, 0u);
        return 5u;
    }
    if (t == 0x80017320u || t == 0x80017368u) {
        d = (int32_t)sext16(a1);
        q = d ? (int32_t)28160 / d : 0;
        if (t == 0x80017320u) {
            WR8(base + 0x2Eu, 1u);
            WR8(base + 0x31u, 0u);
            WR16(base + 0x38u, (uint16_t)q);
            return 1u;
        }
        WR8(base + 0x2Eu, 2u);
        WR16(base + 0x38u, (uint16_t)q);
        return 2u;
    }
    return 0u;
}
