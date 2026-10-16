#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_80017C14(const GuestMem *m, uint32_t a0, uint32_t a1) {
    uint32_t s0 = 0x80079208u;
    uint32_t i;
    uint32_t v0;

    /* 80017C24..80017C2C: andi a0,a0,0xff; beq a0,5 -> return 5, no store */
    if ((a0 & 0xFFu) == 5u) {
        return 5u;
    }
    /* 80017C34..80017C54: copy 8 bytes from a1[i] to s0+0x20+i */
    for (i = 0u; i < 8u; i++) {
        WR8(s0 + 0x20u + i, RD8(a1 + i));
    }
    /* 80017C58: jal 8004CC84, a0 = s0+37; a1 unchanged; a2/a3 not set here (passed as 0) */
    v0 = m->call4(m->ctx, 0x8004CC84u, s0 + 0x25u, a1, 0u, 0u);
    /* 80017C60: sw v0,0x14(s0) */
    WR32(s0 + 0x14u, v0);
    return v0;
}
