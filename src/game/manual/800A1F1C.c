#include "manual.h"
#include "hex2c_rt.h"

/* Falls through into 800A1F34 (no jump). Inlined here because 800A1F34 reads $v0 and $v1 at entry,
 * which the call interface cannot pass. $v1 = *(0x800A34E8), $v0 = (byte at v1+4) < 32, $a1 = 0x80060000. */
uint32_t man_800A1F1C(const GuestMem *m) {
    uint32_t v1, b, r;

    v1 = RD32(0x800A34E8u);
    b = RD8(v1 + 4u);
    if (b < 32u) {
        r = 1u << b;
        WR32(0x80066748u, RD32(0x80066748u) | r);
        return r;
    }
    r = b - 32u;
    WR16(0x8006674Cu, (uint16_t)(RD16(0x8006674Cu) | (1u << (r & 31u))));
    return r;
}
