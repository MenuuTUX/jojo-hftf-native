#include "manual.h"
#include "hex2c_rt.h"

/* 800208D0: 128 iterations. Byte i of the scratchpad table at 0x1F800380 is read in order, its nibbles are swapped
 * ((b >> 4) + (b << 4), full 32-bit value), and the low byte is stored at a0 + 127 - i (descending). The 32-bit value
 * of the last iteration stays in $v0 at jr ra, so the return is the unmasked sum for the byte at 0x1F8003FF (up to 0xFFF). */
uint32_t man_800208D0(const GuestMem *m, uint32_t a0) {
    uint32_t src = 0x1F800380u;
    uint32_t dst = a0 + 127u;
    uint32_t cnt = 128u;
    uint32_t v = 0u;
    uint32_t r = 0u;

    do {
        v = RD8(src);
        src = src + 1u;
        r = (v >> 4) + (v << 4);
        WR8(dst, r);
        dst = dst - 1u;
        cnt = cnt - 1u;
    } while (cnt != 0u);
    return r;
}
