#include "manual.h"
#include "hex2c_rt.h"

/* 8002DA98 (TEXT). If the signed byte at 0x800959E9 is nonzero: +0x6E = table 0x8005B0D4[idx] - 9, where
 * idx is the signed byte at 0x800959EB. Otherwise clears +0 and tail-calls 80025A50(a0, 0).
 * $a2/$a3 pass through to 80025A50 unchanged. */
uint32_t man_8002DA98(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t result;

    (void)a1;
    if (RDS8(0x800959E9u) != 0) {
        result = (uint32_t)RD16(0x8005B0D4u + ((uint32_t)RDS8(0x800959EBu) << 1)) - 9u;
        WR16(a0 + 0x6Eu, result);
        return result;
    }
    WR8(a0, 0u);
    return m->call4(m->ctx, 0x80025A50u, a0, 0u, a2, a3);
}
