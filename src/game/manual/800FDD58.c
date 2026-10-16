#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FDD58(const GuestMem *m, uint32_t a0) {
    uint32_t s1 = a0;
    uint32_t s0;
    uint32_t v;
    int32_t cnt;

    s0 = RD32(s1 + 0x80u);
    /* 800FDD70: jal 80026A04 with $a0 still the original argument */
    m->call4(m->ctx, 0x80026A04u, a0, 0u, 0u, 0u);
    /* 800FDD78..800FDD94: halfword at +0xC decremented and stored; byte at +0x32 of s1 = byte at +0x32 of s0 + 4
     * (the s0 byte is read before the store, as the original's order) */
    cnt = (int16_t)(uint16_t)(RD16(s1 + 0xCu) - 1u);
    v = RD8(s0 + 0x32u) + 4u;
    WR16(s1 + 0xCu, (uint16_t)cnt);
    WR8(s1 + 0x32u, v);
    /* 800FDD98..800FDDB0: returns the sign-extended count unless it is -1, which increments byte+5 */
    if (cnt != -1) {
        return (uint32_t)cnt;
    }
    v = RD8(s1 + 5u) + 1u;
    WR8(s1 + 5u, v);
    return v;
}
