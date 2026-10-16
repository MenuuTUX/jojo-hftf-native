#include "manual.h"
#include "hex2c_rt.h"

/* Same prefix as 800C8AE4. Not-taken path enters 800C8C88 (its prologue is the delay slot) with $a2 = record. */
uint32_t man_800C8C6C(const GuestMem *m, uint32_t a0) {
    uint32_t v, r;

    v = (RD16(a0 + 0xCu) - 1u) & 0xFFFFu;
    WR16(a0 + 0xCu, (uint16_t)v);
    if ((v << 16) == 0u) {
        r = RD8(a0 + 5u) + 1u;
        WR8(a0 + 5u, (uint8_t)r);
        return r;
    }
    return m->call4(m->ctx, 0x800C8C88u, a0, 0u, a0, 0u);
}
