#include "manual.h"
#include "hex2c_rt.h"

/* 80025280 (TEXT). Zeroes 152 bytes at 0x80066CD8 and 152 bytes at 0x80066D70 (the two ranges are adjacent), then sets
 * the byte at 0x80066D72 to 1. Reads no argument register (nargs 0). The original returns the 64-bit value
 * $v0:$v1 = 1 : 0x80060000; this interface returns only $v0 (1). */
uint32_t man_80025280(const GuestMem *m) {
    uint32_t i;

    for (i = 0; i < 0x98u; i++) {
        WR8(0x80066CD8u + i, 0u);
        WR8(0x80066D70u + i, 0u);
    }
    WR8(0x80066D72u, 1u);
    return 1u;
}
