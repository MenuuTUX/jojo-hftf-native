#include "manual.h"
#include "hex2c_rt.h"

/* VS_SEL 800E0850..800E08D8. $a0 = P. Sets P+1, P+0xB0, P+0x32, P+0x1E, P+0x10 and P+0x1D, stores byte P+5 + 1, reads
 * halfword table[2*byte(P+4)] at 0x800E23EC, sets P+0x6E = 102 and P+0x6A = that halfword + 192 (the sh is the jal delay slot,
 * so it runs before the callee), then jal 800258f0 with a0 = P, a1 = 1, and a2/a3 as received; returns its $v0. */
uint32_t man_VS_SEL_800E0850(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;
    uint32_t sp = m->sp_get(m->ctx) - 24u;               /* addiu sp,sp,-24 */
    uint32_t w, t, r;
    m->sp_set(m->ctx, sp);

    WR8(p + 1u, 1u);                                      /* sb v0,1(a0), v0 = 1 */
    WR8(p + 0xB0u, 1u);                                   /* sb v0,0xb0(a0) */
    WR8(p + 0x32u, 12u);                                  /* sb v0,0x32(a0), v0 = 12 */
    WR16(p + 0x1Eu, 1u);                                  /* sh v0,0x1e(a0) */
    WR16(p + 0x10u, 81u);                                 /* sh v1,0x10(a0), v3 = 81 */
    WR8(p + 0x1Du, 0xFFu);                                /* sb v0,0x1d(a0), v0 = -1 */
    WR8(p + 5u, RD8(p + 5u) + 1u);                        /* lbu v0,5(a0); addiu v0,v0,1; sb v0,5(a0) */
    w = RD8(p + 4u);                                      /* lbu v0,4(a0) */
    WR8(p + 0xB1u, 0u);                                   /* sb zero,0xb1(a0) */
    WR8(p + 0xB8u, 0u);                                   /* sb zero,0xb8(a0) */
    t = RD16(0x800E23ECu + 2u * w);                       /* sll v0,v0,1; addu; lhu v0,0(v0) */
    WR16(p + 0x6Eu, 102u);                                /* sh v3,0x6e(a0), v3 = 102 */
    WR16(p + 0x6Au, t + 192u);                            /* addiu v0,v0,192; sh v0,0x6a(a0) (jal delay slot) */
    r = m->call4(m->ctx, 0x800258F0u, p, 1u, a2, a3);    /* jal 800258f0, delay slot addiu a1,zero,1 */

    m->sp_set(m->ctx, sp + 24u);
    return r;
}
