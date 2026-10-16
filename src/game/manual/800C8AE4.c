#include "manual.h"
#include "hex2c_rt.h"

/* Decrements the u16 at +0xC. If it reaches 0, bumps the byte at +5 and returns it (branch to 800C8BB4).
 * Otherwise the delay slot (addiu sp) is the prologue of 800C8B00, which is entered with $a2 = record. */
uint32_t man_800C8AE4(const GuestMem *m, uint32_t a0) {
    uint32_t v, r;

    v = (RD16(a0 + 0xCu) - 1u) & 0xFFFFu;
    WR16(a0 + 0xCu, (uint16_t)v);
    if ((v << 16) == 0u) {
        r = RD8(a0 + 5u) + 1u;
        WR8(a0 + 5u, (uint8_t)r);
        return r;
    }
    return m->call4(m->ctx, 0x800C8B00u, a0, 0u, a0, 0u);
}
