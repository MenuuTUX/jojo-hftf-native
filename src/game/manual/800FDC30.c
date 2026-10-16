#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FDC30(const GuestMem *m, uint32_t a0, uint32_t in_v0) {
    uint32_t s1 = a0;
    uint32_t s0;
    uint32_t v;

    s0 = RD32(s1 + 0x80u);
    /* 800FDC48: bne v0,zero,800FDD44 returns with $v0 unchanged */
    if (in_v0 != 0u) {
        return in_v0;
    }
    /* 800FDC50: jal 80026A04 with $a0 still the original argument */
    m->call4(m->ctx, 0x80026A04u, a0, 0u, 0u, 0u);
    if (RD8(s1 + 6u) != 0u) {
        /* 800FDC68: jal 800B428C with $a0 = s1; then a signed byte at +0x4D of -1 goes to the increment (800FDD34) */
        m->call4(m->ctx, 0x800B428Cu, s1, 0u, 0u, 0u);
        if (RDS8(s1 + 0x4Du) == -1) {
            goto inc;
        }
    } else {
        /* 800FDC88..800FDCE0: +0x6A of s1 = s0 +0x6A + s1 +0x72; +0x32 of s1 -= table[byte +4 of s1] (table at 0x800F4904); +0xEE -= 2 when byte+7 of s0 >= 2 */
        WR16(s1 + 0x6Au, RD16(s0 + 0x6Au) + RD16(s1 + 0x72u));
        WR8(s1 + 0x32u, RD8(s0 + 0x32u) - RD8(0x800F4904u + RD8(s1 + 4u)));
        if (RD8(s0 + 7u) >= 2u) {
            WR16(s1 + 0xEEu, RD16(s1 + 0xEEu) - 2u);
        }
    }
    /* 800FDCE4..800FDD30: all five tests true, including +5 == 1 of s1, returns 1 with $v0 = 1 */
    if (RD8(s0 + 6u) == 0u && RD8(s0 + 0x1FAu) == 8u && RD8(s0 + 0x205u) != 0u && RD8(s0 + 0x206u) == 21u &&
        RD8(s0 + 5u) == 1u) {
        return 1u;
    }
inc:
    /* 800FDD34..800FDD40: byte+5 of s1 is incremented and the new value is returned */
    v = RD8(s1 + 5u) + 1u;
    WR8(s1 + 5u, v);
    return v;
}
