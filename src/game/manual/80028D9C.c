#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800121F0(const GuestMem *m, uint32_t a0, uint32_t a1);

/* 80028D9C (TEXT). Pause-menu text box: a 3-halfword record (160, 146, 2) with two zero bytes at sp+0x10..sp+0x17 is
 * passed to man_800121F0 as $a0. $a1 is the 0x80054924 text when byte 0x80066C67 has bit 0 set, else 0x80054930.
 * $a2 and $a3 are never written, so they pass on to the callee unchanged (the callee does not read them). */
uint32_t man_80028D9C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx) - 0x20u;   /* the original "addiu sp,sp,-32" */
    uint32_t text, r;

    m->sp_set(m->ctx, sp);
    WR16(sp + 0x10u, 160u);                    /* 80028DA0..80028DA4 */
    WR16(sp + 0x12u, 146u);                    /* 80028DA8..80028DAC */
    WR16(sp + 0x14u, 2u);                      /* 80028DB8..80028DC0 */
    WR8(sp + 0x16u, 0u);                       /* 80028DC4 */
    WR8(sp + 0x17u, 0u);                       /* 80028DD0 */
    text = (RD8(0x80066C67u) & 1u) ? 0x80054924u : 0x80054930u;   /* 80028DB0..80028DE8 */
    r = man_800121F0(m, sp + 0x10u, text);     /* 80028DF0 */
    m->sp_set(m->ctx, sp + 0x20u);
    return r;
}
