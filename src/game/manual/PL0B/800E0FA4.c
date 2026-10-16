#include "manual.h"
#include "hex2c_rt.h"

/* PL0B 800E0FA4..800E0FD8 (slot A, base 800DF000). $a0 = P. $a1 is set by lw before the jal, so it is a dummy parameter
 * (nargs 4 keeps $a2 and $a3 as parameters 3 and 4). The jal 800268bc passes $a0 = P, $a1 = lw 0x800E8DA0, and $a2/$a3
 * unchanged from entry. The byte at P+7 is incremented before the call. The return is $v0 as 800268bc leaves it. */
uint32_t man_PL0B_800E0FA4(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;
    uint32_t tbl;
    (void)a1;
    WR8(p + 7u, RD8(p + 7u) + 1u);                      /* lbu v0,7(a0); addiu v0,v0,1; sb v0,7(a0) */
    tbl = RD32(0x800E8DA0u);                            /* lui v0,0x800f; lw a1,-0x7260(v0) */
    return m->call4(m->ctx, 0x800268BCu, p, tbl, a2, a3); /* jal 800268bc, nop delay slot */
}
