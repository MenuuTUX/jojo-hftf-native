#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FCB1C(const GuestMem *m, uint32_t a0) {
    uint32_t a2;
    uint32_t v1;
    uint32_t idx;
    uint32_t ent;
    uint32_t flag;
    uint32_t v0;

    WR16(a0 + 238u, 47u);                   /* 800FCB20..800FCB28 */
    WR16(a0 + 240u, 47u);
    WR8(a0 + 176u, 0x83u);                  /* addiu 131 then sb */
    WR16(a0 + 12u, 12u);

    a2 = RD32(a0 + 0x80u);                  /* lw a2,0x80(a0) */
    v1 = RD16(0x800FFA14u + (RD8(a0 + 4u) << 1));   /* lhu from table at 0x800FFA14 */
    WR16(a0 + 0x6Eu, RD16(a2 + 0x6Eu) + v1);

    idx = RD8(a0 + 4u);
    ent = 0x800FFA1Cu + (idx << 3);         /* table of 8-byte entries at 0x800FFA1C */
    WR32(a0 + 0x7Cu, 0u);
    WR32(a0 + 0x74u, RD32(ent + 4u));
    WR16(a0 + 0x6Au, RD16(a2 + 0x6Au));

    flag = RD8(a0 + 11u);
    if (flag == 0u) {
        /* 800FCB9C..800FCBB8 */
        WR32(a0 + 0x78u, 0xFFFF8000u);
        WR32(a0 + 0x70u, RD32(ent + 0u));
    } else {
        /* 800FCBC0..800FCBE0 */
        v0 = RD32(ent + 0u);
        WR32(a0 + 0x78u, 0x8000u);
        WR32(a0 + 0x70u, 0u - v0);
    }

    /* 800FCBE4..800FCC00 */
    v0 = RD32(RD32(a2 + 0x12Cu) + 4u);
    WR32(a0 + 0x48u, v0);

    /* jal 800268BC (a0 = struct, a1 = off_8010239C, a2 = struct's a2 value, a3 not set here) */
    return m->call4(m->ctx, 0x800268BCu, a0, RD32(0x8010239Cu), a2, 0u);
}
