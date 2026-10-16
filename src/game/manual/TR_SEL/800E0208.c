#include "manual.h"
#include "hex2c_rt.h"

/* TR_SEL 800E0208..800E02CB (196 bytes). $a0 = P (copied to $a1 at entry, then $a0 again before the jal). $a1 is overwritten
 * before any read (dummy parameter). $a2 and $a3 are not written here and pass through to the callee. Stores fields of P,
 * sets the two halfword fields at +0x6A/+0x6E from byte P+4, then jal 800258f0 (a0 = P, a1 = 1) and returns its $v0.
 * Note: the stores at +0x10 and +0x6A/+0x6E run on every path; the bne at 800e0298 selects a0 through its delay slot. */
uint32_t man_TR_SEL_800E0208(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0, sp, w, v, w2, r;
    (void)a1;

    sp = m->sp_get(m->ctx) - 24u;                        /* addiu sp,sp,-24 */
    m->sp_set(m->ctx, sp);
    WR32(sp + 16u, m->gpr_get(m->ctx, 31u));            /* sw ra,0x10(sp) */

    WR8(p + 1u, 1u);                                     /* sb v0,1(a1) with v0 = 1 */
    WR8(p + 0xB0u, 3u);                                  /* sb v0,0xb0(a1) */
    WR8(p + 0x32u, 13u);                                 /* sb v0,0x32(a1) */
    WR16(p + 0x1Eu, 1u);                                 /* sh v0,0x1e(a1) */
    WR8(p + 0x1Du, 0xFFu);                               /* sb v0,0x1d(a1), v0 = -1 */
    WR32(p + 0xACu, 0x800E20D4u);                        /* sw v0,0xac(a1), v0 = 0x800e20d4 (lui/addiu) */
    v = RD8(p + 5u);                                     /* lbu v0,5(a1) */
    w = RD8(p + 4u);                                     /* lbu v1,4(a1) */
    (void)RD8(p + 4u);                                   /* lbu a0,4(a1): a0 is overwritten below, value unused */
    WR8(p + 0xB1u, 0u);                                  /* sb zero,0xb1(a1) */
    WR8(p + 0xB8u, 0u);                                  /* sb zero,0xb8(a1) */
    WR8(p + 5u, v + 1u);                                 /* addiu v0,v0,1; sb v0,5(a1) */
    WR16(p + 0x10u, w);                                  /* sh v1,0x10(a1), delay slot of bne at 800e0264 (both paths) */

    if (w == 0u) {                                       /* beq fall-through of bne at 800e0264 */
        WR16(p + 0x6Au, 192u);                           /* sh v0,0x6a(a1) */
        WR16(p + 0x6Eu, 104u);                           /* j 800e02b0; sh v0,0x6e(a1) in its delay slot */
    } else {
        w2 = RD8(p + 4u);                                /* lbu v1,4(a1) at 800e0280 */
        WR16(p + 0x6Au, 192u);                           /* sh v0,0x6a(a1) */
        WR16(p + 0x6Eu, 40u);                            /* sh v0,0x6e(a1) */
        if (w2 == 2u) {                                  /* bne v1,v0,800e02b4 not taken */
            WR16(p + 0x10u, 6u);                         /* sh v0,0x10(a1) */
            WR8(p + 0x32u, 12u);                         /* sb v0,0x32(a1) */
        }
    }

    r = m->call4(m->ctx, 0x800258F0u, p, 1u, a2, a3);    /* jal 800258f0, a0 = P (addu in delay slot), a1 = 1 in its delay slot */

    m->gpr_set(m->ctx, 31u, RD32(sp + 16u));           /* lw ra,0x10(sp) */
    m->sp_set(m->ctx, sp + 24u);
    return r;
}
