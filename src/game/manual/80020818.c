#include "manual.h"
#include "hex2c_rt.h"

/* 80020818: copies the 32 words at the scratchpad 0x1F800380 (128 bytes) to the guest buffer at $a0, in address order.
 * No calls. Returns the last word read, from 0x1F8003FC (the $v0 the original leaves at jr ra). */
uint32_t man_80020818(const GuestMem *m, uint32_t a0) {
    uint32_t src = 0x1F800380u;   /* $a1 */
    uint32_t dst = a0;            /* $a0, incremented per word */
    uint32_t v0 = 0u;
    uint32_t i;

    for (i = 32u; i != 0u; --i) {
        v0 = RD32(src);
        src += 4u;
        WR32(dst, v0);
        dst += 4u;
    }
    return v0;
}
