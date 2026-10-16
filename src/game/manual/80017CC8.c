#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_80017CC8(const GuestMem *m, uint32_t a0, uint32_t a1) {
    uint32_t i;

    if ((a0 & 0xFFu) == 5u) {
        WR8(0x80079231u, 1u);
        return 1u;
    }
    for (i = 0; i < 8u; i++)
        WR8(0x80079228u + i, RD8(a1 + i));
    WR8(0x8007923Au, 1u);
    return 1u;
}
