#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800121F0(const GuestMem *m, uint32_t a0, uint32_t a1);

/* 800A2B68 (GAMESS). "GAME OVER" text box: record (156, 120, 5) with two zero bytes at sp+0x10..sp+0x17 passed to
 * man_800121F0 as $a0, with $a1 = 0x800A180C (the "GAME OVER" string). $a2 and $a3 are never written and pass on
 * unchanged to the jal (the callee reads only $a0, $a1). */
uint32_t man_800A2B68(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx) - 0x20u;   /* the original "addiu sp,sp,-32" */
    uint32_t r;

    m->sp_set(m->ctx, sp);
    WR16(sp + 0x10u, 156u);
    WR16(sp + 0x12u, 120u);
    WR16(sp + 0x14u, 5u);
    WR8(sp + 0x16u, 0u);
    WR8(sp + 0x17u, 0u);
    r = man_800121F0(m, sp + 0x10u, 0x800A180Cu);
    m->sp_set(m->ctx, sp + 0x20u);
    return r;
}
