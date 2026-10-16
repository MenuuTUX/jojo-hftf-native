#include "manual.h"
#include "hex2c_rt.h"

/* 80020C10: one countdown step over the table at *0x80089F48. Entry count is the u16 at 0x80081BC8. For each entry i the
 * byte at table + 2 + 12*i is decremented when it is non-zero (the original stores byte-1 only on the non-zero path).
 * $v0 is 0 on every path at jr ra (the last writer is the loop's slt result, or the count load when the count is 0). */
uint32_t man_80020C10(const GuestMem *m) {
    uint32_t base, p, i, b;

    base = RD32(0x80089F48u);
    if (RD16(0x80081BC8u) != 0u) {
        p = base + 2u;
        i = 0u;
        for (;;) {
            b = RD8(p);
            if (b != 0u) {
                WR8(p, b - 1u);
            }
            i = i + 1u;
            if (!((int32_t)i < (int32_t)RD16(0x80081BC8u))) {
                break;
            }
            p = p + 12u;
        }
    }
    return 0u;
}
