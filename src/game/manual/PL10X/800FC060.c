#include "manual.h"
#include "hex2c_rt.h"

#define S8_(x) ((uint32_t)(int32_t)(int8_t)(x))

/* 800FC060 (PL10X, slot B, 388 bytes; same instructions as 800E6860 in PL10, only the branch and jal-target addresses differ). $a0 = P. $a1 and $a2 are written (addiu) before the only jal (80025548), and
 * $a3 is passed on to that jal unchanged, so nargs is 4. The result of the jal is only used as a pointer (sw s0,0x80(v0)).
 * $v0 at jr ra is the value stored into P+0x10: 0x2004 when the byte at 0xF8 of the source struct equals $a0 & 0xff,
 * else 0x2003 (the bne at 69b8 tests the values before its delay slot, addiu v0,zero,0x2003, runs). The gen C returned
 * 0x2003 where the reference returns 0x2004. */
uint32_t man_PL10X_800FC060(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;                                 /* s0 */
    uint32_t s1, v0, v1, t;

    v1 = RD8(p + 0x06u);                             /* lbu v1,6(s0) */
    s1 = RD32(p + 0x80u);                            /* lw s1,0x80(s0) */
    if (v1 != 0u) goto L693C;                        /* bne v1,zero,693c (delay slot nop) */

    t = (RD8(s1 + 0x05u) < 2u);                      /* lbu v0,5(s1); sltiu v0,v0,2 */
    v0 = v1 + 1u;                                    /* addiu v0,v1,1: beq delay slot, runs on both paths */
    if (t == 0u) goto L6950;                         /* beq v0(=t),zero,6950 */
    WR8(p + 0x06u, v0);                              /* sb v0,6(s0) */
    WR16(p + 0x72u, RD16(s1 + 0x6Au));              /* lhu v0,0x6a(s1); sh v0,0x72(s0) */
    v1 = S8_(RD8(p + 0x88u));                        /* lb v1,0x88(s0) (overwritten by the slti below) */
    WR16(p + 0x76u, RD16(s1 + 0x6Eu));              /* lhu v0,0x6e(s1); sh v0,0x76(s0) */
    WR16(p + 0xC2u, RD8(s1 + 0xF8u));               /* lbu v0,0xf8(s1); sh v0,0xc2(s0) */
    WR16(p + 0x7Au, RD16(s1 + 0x6Au));              /* lhu v0,0x6a(s1); sh v0,0x7a(s0) */
    WR16(p + 0x7Eu, RD16(s1 + 0x6Eu));              /* lhu v0,0x6e(s1); sh v0,0x7e(s0) */
    WR16(p + 0xC6u, RD8(s1 + 0xF8u));               /* lbu v0,0xf8(s1); sh v0,0xc6(s0) */
    WR16(p + 0x6Au, RD16(s1 + 0x6Au));              /* lhu v0,0x6a(s1); sh v0,0x6a(s0) */
    v0 = RD16(s1 + 0x6Au);                          /* lhu v0,0x6a(s1) */
    v1 = ((int32_t)v1 < 3);                          /* slti v1,v1,3 */
    WR16(p + 0x6Eu, v0);                            /* sh v0,0x6e(s0) */
    a0 = RD8(s1 + 0xF8u);                           /* lbu a0,0xf8(s1) */
    v0 = 1u;
    WR8(p + 0x01u, v0);                             /* sb v0,1(s0) */
    v0 = 0x2004u;                                   /* addiu v0,zero,8196 */
    WR16(p + 0x10u, v0);                            /* sh v0,0x10(s0) */
    WR8(p + 0xF8u, a0);                             /* beq v1,zero delay slot: sb a0,0xf8(s0) on both paths */
    if (v1 == 0u) goto L693C;

    a0 = (RD8(p + 0x100u) + 4u) & 0xFFu;           /* lbu a0,0x100(s0); addiu a0,a0,4; andi a0,a0,0xff */
    a1 = 2u;                                        /* addiu a1,zero,2 */
    a2 = a1;                                        /* addu a2,a1,zero */
    v0 = m->call4(m->ctx, 0x80025548u, a0, a1, a2, a3);   /* jal 80025548 */
    if (v0 == 0u) goto L693C;                       /* beq v0,zero,693c */
    WR32(v0 + 0x80u, p);                            /* sw s0,0x80(v0): delay-free, reached only on the non-zero path */

L693C:
    t = (RD8(s1 + 0x05u) < 2u);                     /* lbu v0,5(s1); sltiu v0,v0,2 */
    v0 = t;
    if (v0 != 0u) goto L6964;                       /* bne v0,zero,6964 */

L6950:
    v0 = RD8(p + 0x05u) + 1u;                       /* lbu v0,5(s0); addiu v0,v0,1 */
    WR8(p + 0x05u, v0);                             /* sb v0,5(s0): j 69d0 delay slot */
    return v0;

L6964:
    v0 = RD16(p + 0x7Au);                           /* lhu v0,0x7a(s0) */
    v1 = RD16(p + 0x7Eu);                           /* lhu v1,0x7e(s0) */
    a0 = RD8(p + 0xC6u);                            /* lbu a0,0xc6(s0) */
    a1 = RD16(p + 0x72u);                           /* lhu a1,0x72(s0) */
    a2 = RD16(p + 0x76u);                           /* lhu a2,0x76(s0) */
    a3 = RD16(p + 0xC2u);                           /* lhu a3,0xc2(s0) */
    WR16(p + 0x6Au, v0);                            /* sh v0,0x6a(s0) */
    WR16(p + 0x6Eu, v1);                            /* sh v1,0x6e(s0) */
    WR8(p + 0xF8u, a0);                             /* sb a0,0xf8(s0) */
    WR16(p + 0x7Au, a1);                            /* sh a1,0x7a(s0) */
    WR16(p + 0x7Eu, a2);                            /* sh a2,0x7e(s0) */
    WR16(p + 0xC6u, a3);                            /* sh a3,0xc6(s0) */
    WR16(p + 0x72u, RD16(s1 + 0x6Au));              /* lhu v0,0x6a(s1); sh v0,0x72(s0) */
    WR16(p + 0x76u, RD16(s1 + 0x6Eu));              /* lhu v0,0x6e(s1); sh v0,0x76(s0) */
    WR16(p + 0xC2u, RD8(s1 + 0xF8u));               /* lbu v0,0xf8(s1); sh v0,0xc2(s0) */
    v0 = RD8(s1 + 0xF8u);                           /* lbu v0,0xf8(s1) at 69b8 */
    v1 = a0 & 0xFFu;                                /* andi v1,a0,0xff */
    /* bne v1,v0,69cc with delay slot addiu v0,zero,8195: the branch tests the loaded byte */
    v0 = (v1 != v0) ? 0x2003u : 0x2004u;
    WR16(p + 0x10u, v0);                            /* sh v0,0x10(s0) */
    return v0;
}
