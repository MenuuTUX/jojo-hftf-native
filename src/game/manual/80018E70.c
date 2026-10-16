#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_80018E70(const GuestMem *m, uint32_t a0) {
    uint32_t s0 = a0;
    uint32_t v3;
    uint32_t result;

    /* 80018E94: jal 8004CB80 (DsIntToPos), a0 = [s0+12], a1 = s0+8; a2/a3 not set here (passed as 0) */
    m->call4(m->ctx, 0x8004CB80u, RD32(s0 + 12u), s0 + 8u, 0u, 0u);

    /* 80018E9C..80018EB4: loop until 80049824 returns nonzero.
       Args: a0 = byte [s0+42], a1 = s0+8, a2 = 6, a3 = 0x8001824C, 5th = -1 at sp+16 */
    do {
        v3 = m->call5(m->ctx, 0x80049824u, RD8(s0 + 42u), s0 + 8u, 6u, 0x8001824Cu, 0xFFFFFFFFu);
    } while (v3 == 0u);

    /* 80018EC0..80018ECC: byte [s0+40] = 4; byte [s0+68] += 1; return the new value (unmasked) */
    WR8(s0 + 40u, 4u);
    result = RD8(s0 + 68u) + 1u;
    WR8(s0 + 68u, result);
    return result;
}
