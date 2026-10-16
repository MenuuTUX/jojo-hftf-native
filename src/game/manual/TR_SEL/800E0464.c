#include "manual.h"
#include "hex2c_rt.h"

/* TR_SEL 800E0464..800E04E7 (136 bytes). $a0 = P, unchanged to the jal. $a1 = 1 (addiu in the body, before the jal). $a2 and
 * $a3 pass through. Reads byte P+5 and byte P+4, indexes the halfword table at 0x800E1FCC by 2 * (byte P+4), and stores
 * (table + 192) to P+0x6A in the delay slot of jal 800258f0, which is called with a0 = P and a1 = 1. Returns the callee's $v0. */
uint32_t man_TR_SEL_800E0464(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0, sp, v, w, h, r;
    (void)a1;

    sp = m->sp_get(m->ctx) - 24u;                        /* addiu sp,sp,-24 */
    m->sp_set(m->ctx, sp);
    WR32(sp + 16u, m->gpr_get(m->ctx, 31u));            /* sw ra,0x10(sp) */

    WR8(p + 1u, 1u);                                     /* sb v0,1(a0), v0 = 1 */
    WR8(p + 0xB0u, 1u);                                  /* sb v0,0xb0(a0) */
    WR8(p + 0x32u, 12u);                                 /* sb v0,0x32(a0), v0 = 12 */
    WR16(p + 0x1Eu, 1u);                                 /* sh v0,0x1e(a0), v0 = 1 */
    WR8(p + 0x1Du, 0xFFu);                               /* sb v0,0x1d(a0), v0 = -1 */
    WR16(p + 0x10u, 81u);                                /* sh v1,0x10(a0), v1 = 81 */
    v = RD8(p + 5u);                                     /* lbu v0,5(a0) at 800e049c */
    WR8(p + 5u, v + 1u);                                 /* addiu v0,v0,1; sb v0,5(a0) */
    w = RD8(p + 4u);                                     /* lbu v0,4(a0) at 800e04ac */
    WR8(p + 0xB1u, 0u);                                  /* sb zero,0xb1(a0) */
    WR8(p + 0xB8u, 0u);                                  /* sb zero,0xb8(a0) */
    h = RD16(0x800E1FCCu + 2u * w);                      /* sll v0,v0,1; addu with v1 = 0x800e1fcc; lhu v0,0(v0) */
    WR16(p + 0x6Eu, 102u);                               /* sh v1,0x6e(a0), v1 = 102 */
    WR16(p + 0x6Au, h + 192u);                           /* addiu v0,v0,192; jal delay slot: sh v0,0x6a(a0) */

    r = m->call4(m->ctx, 0x800258F0u, p, 1u, a2, a3);   /* jal 800258f0, a1 = 1 from the body */

    m->gpr_set(m->ctx, 31u, RD32(sp + 16u));           /* lw ra,0x10(sp) */
    m->sp_set(m->ctx, sp + 24u);
    return r;
}
