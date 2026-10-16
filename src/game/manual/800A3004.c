#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800121F0(const GuestMem *m, uint32_t a0, uint32_t a1);

/* 800A3004: builds a 6-byte record in its own frame (0x20 bytes, record at sp+0x10..sp+0x17)
   and calls the text helper with the record and the string at 0x800A1818 ("EXPLAIN").
   Returns the helper's $v0. The frame is reproduced in guest memory because the record's
   address is passed to the callee. */
uint32_t man_800A3004(const GuestMem *m) {
    uint32_t sp = m->sp_get(m->ctx) - 0x20u;
    uint32_t r;

    m->sp_set(m->ctx, sp);
    WR16(sp + 0x10u, 164u);
    WR16(sp + 0x12u, 120u);
    WR16(sp + 0x14u, 5u);
    WR8(sp + 0x16u, 0u);
    WR8(sp + 0x17u, 0u);
    r = man_800121F0(m, sp + 0x10u, 0x800A1818u);
    m->sp_set(m->ctx, sp + 0x20u);
    return r;
}
