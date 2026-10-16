#include "manual.h"
#include "hex2c_rt.h"

/* PL04 800E14D4..800E150C (56 bytes, slot A base 800DF000). Leaf-with-call: increments the byte at a0+7, then
 * jal 800268BC with $a0 = a0 (unchanged), $a1 = RD32(0x800EAC10), $a2 and $a3 unchanged from the caller.
 * Returns the callee's $v0. The generated C read RD32(0x800ECE40); the lui v0,0x800f / lw a1,-0x53f0(v0) pair
 * addresses 0x800EAC10. */
uint32_t man_PL04_800E14D4(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t v0, arg1;

    (void)a1;                                       /* $a1 is written by the lw before any use */
    WR8(a0 + 7u, RD8(a0 + 7u) + 1u);                /* lbu v0,0x7(a0); addiu v0,v0,1; sb v0,0x7(a0) */
    arg1 = RD32(0x800EAC10u);                       /* lui v0,0x800f; lw a1,-0x53f0(v0) */
    v0 = m->call4(m->ctx, 0x800268BCu, a0, arg1, a2, a3);   /* jal 800268bc; delay slot nop */
    return v0;
}
