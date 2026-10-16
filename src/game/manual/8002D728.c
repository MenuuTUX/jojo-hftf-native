#include "manual.h"
#include "hex2c_rt.h"

/* 8002D728 (TEXT). Leaf: stores 1 at a0+1 when the byte at 0x8008EBC2 is nonzero, else 0. Returns 1
 * ($v0 is 1 on both paths: it is set in the delay slot of the bne at 8002D734). */
uint32_t man_8002D728(const GuestMem *m, uint32_t a0) {
    WR8(a0 + 1u, RD8(0x8008EBC2u) != 0u ? 1u : 0u);
    return 1u;
}
