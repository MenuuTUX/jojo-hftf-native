#include "manual.h"
#include "hex2c_rt.h"

/* VS_SEL 800DF778..800DF814. s0 = 0x80066C48. Decrements halfword s0+0xC; if the result is non-zero, jal 80025d40 and, if its
 * $v0 & 0xFFFF is zero, goto 800df7d8. Otherwise (the 800df7bc path) halfword s0+2 += 1 and jal 80023a60, returning its $v0.
 * 800df7d8: jal 800e0194; then jal 800e0474 with $a0 = s0 (0x800E2F54), jal 800e0474 with $a0 = s0+32, jal 80029300 with
 * $a0 = 7, then jal 8001f0a8 and return its $v0. Each later call takes $a0..$a3 as the previous callee left them, read back
 * with gpr_get after call4 (the MIPS leaves those registers at the jal). Guest $s0 is not modelled (as in the other manual files). */

static void reload_args(const GuestMem *m, uint32_t *a0, uint32_t *a1, uint32_t *a2, uint32_t *a3) {
    *a0 = m->gpr_get(m->ctx, 4u);
    *a1 = m->gpr_get(m->ctx, 5u);
    *a2 = m->gpr_get(m->ctx, 6u);
    *a3 = m->gpr_get(m->ctx, 7u);
}

uint32_t man_VS_SEL_800DF778(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx) - 24u;               /* addiu sp,sp,-24 */
    uint32_t s0 = 0x80066C48u;                            /* lui v0,0x8006; addiu s0,v0,27720 */
    uint32_t h, v0, r;
    m->sp_set(m->ctx, sp);

    h = (RD16(s0 + 0xCu) - 1u) & 0xFFFFu;                 /* lhu v0,0xc(s0); addiu v0,v0,-1 */
    WR16(s0 + 0xCu, h);                                   /* sh v0,0xc(s0) */
    if (h != 0u) {                                        /* sll v0,v0,16; beq v0,zero,800df7bc */
        r = m->call4(m->ctx, 0x80025D40u, a0, a1, a2, a3);   /* jal 80025d40, delay nop */
        reload_args(m, &a0, &a1, &a2, &a3);
        r &= 0xFFFFu;                                     /* andi v0,v0,0xffff */
        if (r == 0u)                                      /* beq v0,zero,800df7d8 */
            goto L_7d8;
    }

    /* 800df7bc: falls through from the call above, or is entered directly when h == 0 */
    v0 = RD16(s0 + 2u) + 1u;                             /* lhu v0,2(s0); addiu v0,v0,1 */
    WR16(s0 + 2u, v0);                                    /* sh v0,2(s0): delay slot of jal 80023a60 */
    r = m->call4(m->ctx, 0x80023A60u, a0, a1, a2, a3);
    m->sp_set(m->ctx, sp + 24u);                          /* j 800df804 (epilogue) */
    return r;

L_7d8:
    (void)m->call4(m->ctx, 0x800E0194u, a0, a1, a2, a3);  /* jal 800e0194; delay lui s0,0x800e */
    reload_args(m, &a0, &a1, &a2, &a3);
    s0 = 0x800E2F54u;                                     /* addiu s0,s0,12116 (after the return) */
    (void)m->call4(m->ctx, 0x800E0474u, s0, a1, a2, a3);  /* jal 800e0474; delay addu a0,s0,zero */
    reload_args(m, &a0, &a1, &a2, &a3);
    (void)m->call4(m->ctx, 0x800E0474u, s0 + 32u, a1, a2, a3);   /* jal 800e0474; delay addiu a0,s0,32 */
    reload_args(m, &a0, &a1, &a2, &a3);
    (void)m->call4(m->ctx, 0x80029300u, 7u, a1, a2, a3);  /* jal 80029300; delay addiu a0,zero,7 */
    reload_args(m, &a0, &a1, &a2, &a3);
    r = m->call4(m->ctx, 0x8001F0A8u, a0, a1, a2, a3);    /* jal 8001f0a8; delay nop */
    m->sp_set(m->ctx, sp + 24u);
    return r;
}
