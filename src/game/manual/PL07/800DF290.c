#include "manual.h"
#include "hex2c_rt.h"

/* PL07 800DF290..800DF2C8 (slot A, base 800DF000). $a0 = P. $a1 is set to 0 in the delay slot of the first jal before any
 * read, so it is a dummy parameter (nargs 4 keeps $a2 and $a3 as parameters 3 and 4).
 * jal 800bb2a4 with $a0 = P, $a1 = 0, $a2 and $a3 as the entry left them.
 * jal 800268bc with $a0 = P, $a1 = lw 0x800E59C0, $a2 and $a3 as the first callee left them (gpr_get after call4).
 * The return is $v0 as 800268bc leaves it. */
uint32_t man_PL07_800DF290(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;                                    /* s0 = a0, 800df298 */
    uint32_t tbl;
    (void)a1;
    m->call4(m->ctx, 0x800BB2A4u, p, 0u, a2, a3);       /* jal 800bb2a4, delay addu a1,zero,zero (800df2a0) */
    a2 = m->gpr_get(m->ctx, 6u);                        /* $a2 as the callee left it */
    a3 = m->gpr_get(m->ctx, 7u);                        /* $a3 as the callee left it */
    tbl = RD32(0x800E59C0u);                            /* lui v0,0x800e; lw a1,0x59c0(v0) (800df2a8) */
    return m->call4(m->ctx, 0x800268BCu, p, tbl, a2, a3); /* jal 800268bc, delay addu a0,s0,zero (800df2b0) */
}
