#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800F749C(const GuestMem *m, uint32_t a0) {
    uint32_t p = a0;
    uint32_t dst = RD32(p + 0x12Cu);
    uint32_t src = 0x800FF544u;
    uint32_t i;
    uint32_t result;
    uint32_t v7;
    uint32_t v8;

    for (i = 0; i < 28u; i++) {
        WR32(dst, RD32(src));
        src += 4u;
        dst += 4u;
    }
    result = RD8(p + 0x136u);
    v7 = RD16(p + 0x1Eu);
    WR8(p + 0xB0u, 0u);
    v8 = RD8(p + 0xB0u);
    WR8(p + 0xB8u, 0u);
    WR8(p + 0xB1u, result);
    WR16(p + 0x84u, v7);
    WR8(p + 0xBAu, v8);
    return result;
}
