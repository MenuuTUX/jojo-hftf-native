#include "manual.h"
#include "hex2c_rt.h"

/* 800E2600 (PL00). $a0 = S (struct). Copies the 28 words at 0x800E7BC8 to the word array at *(S+0x12C), then clears
 * the bytes S+0xB0, S+0xBA, S+0xB8, stores the byte S+0x136 at S+0xB1 and returns it. */
uint32_t man_800E2600(const GuestMem *m, uint32_t a0) {
    uint32_t dst = RD32(a0 + 0x12Cu);
    uint32_t i;
    uint32_t v;

    for (i = 0; i < 28u; i++)
        WR32(dst + 4u * i, RD32(0x800E7BC8u + 4u * i));

    v = RD8(a0 + 0x136u);
    WR8(a0 + 0xB0u, 0u);
    WR8(a0 + 0xBAu, 0u);
    WR8(a0 + 0xB8u, 0u);
    WR8(a0 + 0xB1u, v);
    return v;
}
