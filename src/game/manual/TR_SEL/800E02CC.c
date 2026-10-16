#include "manual.h"
#include "hex2c_rt.h"

/* TR_SEL 800E02CC..800E03CB (256 bytes). $a0 = P, kept unchanged to the jal. $a1 is loaded from P+0x80 before the jal (the
 * jal's delay slot sets $a1 = 1, which is what the callee sees). $a2 = 1 (addiu at 800e0300), $a3 passes through.
 * S = byte at 0x80066C6A. Byte P+4 is read twice (w at 800e02d4, then w2 at 800e0320).
 *   w2 == 0: P+1 = (S & 1) ? 1 : 0; P+0x6A = 122, P+0x6E = 178.
 *   w2 == 1: P+1 = (S & 1) ? 0 : 1; P+0x6A = 262, P+0x6E = 178.
 *   otherwise: P+1 = 0; P+0x10 = (byte at a1 & 1) + 79 (a1 = the pointer at P+0x80).
 * P+0x10 = (w & 1) + 79 is stored on every path before the split.
 * Then jal 800258f0 (a0 = P, a1 = 1, a2 = 1, a3 as received); returns its $v0. */
uint32_t man_TR_SEL_800E02CC(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0, sp, w, v, w2, a1p, s, r;
    (void)a1;
    (void)a2;

    sp = m->sp_get(m->ctx) - 24u;                        /* addiu sp,sp,-24 */
    m->sp_set(m->ctx, sp);
    WR32(sp + 16u, m->gpr_get(m->ctx, 31u));            /* sw ra,0x10(sp) */

    w = RD8(p + 4u);                                     /* lbu v1,4(a0) at 800e02d4 */
    WR8(p + 0xB0u, 1u);                                  /* sb v0,0xb0(a0), v0 = 1 */
    WR8(p + 0x32u, 11u);                                 /* sb v0,0x32(a0), v0 = 11 */
    WR8(p + 0x1Du, 0xFFu);                               /* sb v0,0x1d(a0), v0 = -1 */
    WR16(p + 0xCu, 2u);                                  /* sh v0,0xc(a0), v0 = 2 */
    v = RD8(p + 5u);                                     /* lbu v0,5(a0) at 800e02f8 */
    a1p = RD32(p + 0x80u);                               /* lw a1,0x80(a0) at 800e02fc */
    WR8(p + 0xB1u, 0u);                                  /* sb zero,0xb1(a0) */
    WR8(p + 0xB8u, 0u);                                  /* sb zero,0xb8(a0) */
    WR16(p + 0x1Eu, 1u);                                 /* sh a2,0x1e(a0), a2 = 1 */
    WR16(p + 0xEu, 0u);                                  /* sh zero,0xe(a0) */
    WR16(p + 0x10u, (w & 1u) + 79u);                     /* andi/addiu 79 then sh v1,0x10(a0) */
    w2 = RD8(p + 4u);                                    /* lbu v1,4(a0) at 800e0320 */
    WR8(p + 5u, v + 1u);                                 /* addiu v0,v0,1; sb v0,5(a0) (bne delay slot) */

    if (w2 == 0u) {                                      /* fall-through of bne at 800e0328 */
        if (RD8(0x80066C6Au) & 1u) WR8(p + 1u, 1u);      /* lbu v0,0x6c6a(v0) via lui 0x8006; beq at 800e0340 not taken: sb a2,1(a0) */
        else                      WR8(p + 1u, 0u);       /* beq taken to 800e0350: sb zero,1(a0) */
        WR16(p + 0x6Au, 122u);                           /* 800e0354 j 800e038c (delay addiu v0,zero,122), sh v0,0x6a */
        WR16(p + 0x6Eu, 178u);                           /* addiu v0,zero,178; j 800e03b4 with sh v0,0x6e in the delay slot */
    } else if (w2 == 1u) {                               /* bne v1,v0,800e039c not taken */
        if (RD8(0x80066C6Au) & 1u) WR8(p + 1u, 0u);      /* bne/beq at 800e0374: bit set -> sb zero,1(a0) in j delay */
        else                      WR8(p + 1u, 1u);       /* bit clear -> sb a2,1(a0) at 800e0384 */
        WR16(p + 0x6Au, 262u);                           /* 800e0388 addiu v0,zero,262; sh v0,0x6a */
        WR16(p + 0x6Eu, 178u);                           /* j 800e03b4; sh v0,0x6e in its delay slot */
    } else {
        WR8(p + 1u, 0u);                                 /* 800e039c sb zero,1(a0) */
        s = RD8(a1p);                                    /* lbu v0,0(a1) at 800e03a0 */
        WR16(p + 0x10u, (s & 1u) + 79u);                 /* andi v0,v0,1; addiu v0,v0,79; sh v0,0x10(a0) */
    }

    r = m->call4(m->ctx, 0x800258F0u, p, 1u, 1u, a3);    /* jal 800258f0, delay addiu a1,zero,1; a2 = 1 */

    m->gpr_set(m->ctx, 31u, RD32(sp + 16u));           /* lw ra,0x10(sp) */
    m->sp_set(m->ctx, sp + 24u);
    return r;
}
