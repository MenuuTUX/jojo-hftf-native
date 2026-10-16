#include "manual.h"
#include "hex2c_rt.h"

/* PL07 800DF598..800DF5DC (slot A, base 800DF000). $a0 = P. $a1 is set in the delay slot of the first jal before any read,
 * so it is a dummy parameter (nargs 4 keeps $a2 and $a3 as parameters 3 and 4).
 * jal 800bb2a4 ($a1 = 19), jal 80024104 ($a0 = P, $a1 = 273), jal 800268bc ($a0 = P, $a1 = lw 0x800E59EC).
 * $a2 and $a3 at each jal are the values the previous callee left (gpr_get after call4); the first jal gets the entry values.
 * The return is $v0 as 800268bc leaves it. */
uint32_t man_PL07_800DF598(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;                                    /* s0 = a0, 800df5a0 */
    uint32_t tbl;
    (void)a1;
    m->call4(m->ctx, 0x800BB2A4u, p, 19u, a2, a3);     /* jal 800bb2a4, delay addiu a1,zero,19 (800df5a8) */
    a2 = m->gpr_get(m->ctx, 6u);
    a3 = m->gpr_get(m->ctx, 7u);
    m->call4(m->ctx, 0x80024104u, p, 273u, a2, a3);    /* addu a0,s0,zero (800df5b0); jal 80024104, delay a1 = 0x111 */
    a2 = m->gpr_get(m->ctx, 6u);
    a3 = m->gpr_get(m->ctx, 7u);
    tbl = RD32(0x800E59ECu);                            /* lui v0,0x800e; lw a1,0x59ec(v0) (800df5bc) */
    return m->call4(m->ctx, 0x800268BCu, p, tbl, a2, a3); /* jal 800268bc, delay addu a0,s0,zero (800df5c4) */
}
