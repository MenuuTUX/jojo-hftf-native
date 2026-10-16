#include "manual.h"
#include "hex2c_rt.h"

#define S8_(x) ((uint32_t)(int32_t)(int8_t)(x))

/* 800F652C (PL17X, slot B, 544 bytes). $a0 = P. $a1 is passed on unchanged to the 800af7ec / 80026a04 callees on some
 * paths and $a3 likewise, and $a2 is set to 1 at entry (addiu a2,zero,1), so nargs is 4. Every callee may change
 * $a0..$a3; they are reloaded with gpr_get after each call. $s1 is not set on the path to 65cc (sb s1,4(v1)), so it is
 * the caller's $s1 (gpr_get at entry). $v0 at jr ra is the callee's $v0 on the paths that end in the jal 80026a04 at 672c,
 * and 0 on the path through 66fc (slti v0,s1,4 with the bne not taken). Branch delay slots run on both paths; where the
 * delay slot writes a register the branch tests, the branch reads the value from before it: the slti at 6550 (in the
 * delay slot of beq 654c) feeds beq 6554, and addiu v0,zero,3 at 6570 runs after beq 656c compares v0 = 2. */
uint32_t man_PL17X_800F652C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;                                /* s0 */
    uint32_t s1, s2, v0, v1, t;

#define REFRESH() do {                                       \
        a0 = m->gpr_get(m->ctx, 4u);                         \
        a1 = m->gpr_get(m->ctx, 5u);                         \
        a2 = m->gpr_get(m->ctx, 6u);                         \
        a3 = m->gpr_get(m->ctx, 7u);                         \
    } while (0)

    s1 = m->gpr_get(m->ctx, 17u);                   /* caller's $s1 (not written on every path) */
    v1 = RD8(p + 0x08u);                            /* lbu v1,8(s0) */
    a2 = 1u;                                        /* addiu a2,zero,1 */
    t = ((int32_t)v1 < 2);                          /* slti v0,v1,2: delay slot of beq 654c, runs on both paths */
    if (v1 == a2) goto L65D0;                       /* beq v1,a2,65d0 */
    v0 = 2u;                                        /* addiu v0,zero,2: delay slot of beq 6554 */
    if (t == 0u) goto L656C;                        /* beq v0(=slti),zero,656c */
    /* 655c */
    if (v1 == 0u) goto L6584;                       /* beq v1,zero,6584 (a0 = P in the delay slot) */
    goto L6714;                                     /* j 6714 (not reached: v1 is 0 or 1 here) */

L656C:
    t = (v1 == v0);                                 /* beq v1,v0(=2),666c */
    v0 = 3u;                                        /* addiu v0,zero,3: delay slot */
    if (t) goto L666C;
    t = (v1 == v0);                                 /* beq v1,v0(=3),66b0 */
    v0 = 4u;                                        /* addiu v0,zero,4: delay slot */
    if (t) goto L66B0;
    goto L6714;                                     /* j 6714 */

    /* 6584: byte 8 = 1, halfword 0xe = 0, then the calls for v1 == 0 */
L6584:
    a1 = 44u;                                       /* addiu a1,zero,44 */
    WR8(p + 0x08u, a2);                             /* sb a2,8(s0) */
    WR16(p + 0x0Eu, 0u);                            /* sh zero,0xe(s0): delay slot of jal 800bb2a4 */
    v0 = m->call4(m->ctx, 0x800BB2A4u, p, a1, a2, a3);
    REFRESH();
    a1 = RD32(0x800FD128u);                         /* lui v0,0x8010; lw a1,-0x2ed8(v0) */
    a0 = p;                                         /* addu a0,s0,zero */
    v0 = m->call4(m->ctx, 0x800268BCu, a0, a1, a2, a3);
    REFRESH();
    a1 = 2u;                                        /* addiu a1,zero,2 */
    a2 = 6u;                                        /* addiu a2,zero,6 */
    a0 = (RD8(p + 0x136u) + 4u) & 0xFFu;            /* lbu a0,0x136(s0); addiu a0,a0,4; andi a0,a0,0xff */
    v0 = m->call4(m->ctx, 0x80025548u, a0, a1, a2, a3);
    REFRESH();
    v1 = v0;                                        /* addu v1,v0,zero */
    if (v1 == 0u) goto L65D0;                       /* beq v1,zero,65d0 */
    WR32(v1 + 0x80u, p);                            /* sw s0,0x80(v1) */
    WR8(v1 + 0x04u, s1);                            /* sb s1,4(v1) */

L65D0:
    v1 = S8_(RD8(p + 0x4Du));                       /* lb v1,0x4d(s0) */
    v0 = 2u;                                        /* addiu v0,zero,2 */
    t = (v1 != v0);                                 /* bne v1,v0,6650 (tested before the delay slot) */
    v0 = 3u;                                        /* addiu v0,zero,3: delay slot, runs on both paths */
    if (t) goto L6650;

    /* 65e0: v1 == 2 */
    a0 = p;                                         /* addu a0,s0,zero */
    a1 = 292u;                                      /* addiu a1,zero,292 */
    v0 = m->call4(m->ctx, 0x80024104u, a0, a1, a2, a3);
    REFRESH();
    a0 = p;                                         /* addu a0,s0,zero */
    a1 = 13u;                                       /* addiu a1,zero,13 */
    a2 = 8u;                                        /* addiu a2,zero,8 */
    v0 = m->call4(m->ctx, 0x80029714u, a0, a1, a2, a3);
    REFRESH();
    a1 = 0u;                                        /* addu a1,zero,zero */
    a0 = (RD8(p + 0x136u) + 2u) & 0xFFu;            /* lbu a0,0x136(s0); addiu a0,a0,2; andi */
    a2 = 12u;                                       /* addiu a2,zero,12 */
    v0 = m->call4(m->ctx, 0x80025548u, a0, a1, a2, a3);
    REFRESH();
    v1 = v0;
    if (v1 == 0u) goto L672C;                       /* beq v1,zero,672c */
    WR32(v1 + 0x80u, p);                            /* sw s0,0x80(v1) */
    WR8(v1 + 0x04u, RD8(p + 0x18Cu));              /* lbu v0,0x18c(s0); sb v0,4(v1) */
    WR8(v1 + 0x88u, RD8(p + 0x0Eu));               /* lbu v0,0xe(s0); sb v0,0x88(v1) */
    v0 = RD16(p + 0x0Eu) + 1u;                      /* lhu v0,0xe(s0); addiu v0,v0,1 */
    WR16(p + 0x0Eu, v0);                            /* j 672c; sh v0,0xe(s0) in the delay slot */
    goto L672C;

L6650:
    if (v1 != v0) goto L672C;                       /* bne v1,v0,672c (v0 = 3 from the delay slot) */
    v0 = RD8(p + 0x08u) + 1u;                       /* lbu v0,8(s0); addiu v0,v0,1 */
    WR8(p + 0x08u, v0);                             /* j 672c; sb v0,8(s0) in the delay slot */
    goto L672C;

    /* 666c: v1 == 2 */
L666C:
    v0 = S8_(RD8(p + 0x4Du));                       /* lb v0,0x4d(s0) */
    t = (v0 == 0u);                                 /* beq v0,zero,672c */
    v0 = 3u;                                        /* addiu v0,zero,3: delay slot, runs on both paths */
    if (t) goto L672C;
    WR8(p + 0x08u, v0);                             /* sb v0,8(s0) */
    a0 = p;                                         /* addu a0,s0,zero */
    a1 = 277u;                                      /* addiu a1,zero,277 */
    v0 = m->call4(m->ctx, 0x80024104u, a0, a1, a2, a3);
    REFRESH();
    a0 = p;                                         /* addu a0,s0,zero */
    a1 = (uint32_t)(int32_t)-81;                    /* addiu a1,zero,-81 */
    a2 = 131u;                                      /* addiu a2,zero,131: delay slot of jal 800bc074 */
    v0 = m->call4(m->ctx, 0x800BC074u, a0, a1, a2, a3);
    REFRESH();
    a0 = p;                                         /* addu a0,s0,zero */
    a1 = 1u;                                        /* addiu a1,zero,1: delay slot of jal 800bb530 */
    v0 = m->call4(m->ctx, 0x800BB530u, a0, a1, a2, a3);
    REFRESH();
    goto L672C;

    /* 66b0: v1 == 3 (v0 = 4 from the delay slot of beq 6574) */
L66B0:
    WR8(p + 0x08u, v0);                             /* sb v0,8(s0) */
    s1 = 0u;                                        /* addu s1,zero,zero */
    s2 = 1u;                                        /* addiu s2,zero,1 */
    a1 = 2u;                                        /* addiu a1,zero,2 */
L66C0:
    a2 = 5u;                                        /* addiu a2,zero,5 */
    a0 = (RD8(p + 0x136u) + 4u) & 0xFFu;            /* lbu a0,0x136(s0); addiu a0,a0,4; andi */
    v0 = m->call4(m->ctx, 0x80025548u, a0, a1, a2, a3);
    REFRESH();
    v1 = v0;
    if (v1 == 0u) goto L66FC;                       /* beq v1,zero,66fc */
    WR32(v1 + 0x80u, p);                            /* sw s0,0x80(v1) */
    WR8(v1 + 0x04u, (RD8(p + 0x18Cu) << 4) | s1);  /* lbu v0,0x18c(s0); sll; or s1; sb v0,4(v1) */
    WR8(p + 0x18Eu, s2);                            /* sb s2,0x18e(s0) */
L66FC:
    s1 = s1 + 1u;                                   /* addiu s1,s1,1 */
    v0 = (s1 < 4u);                                 /* slti v0,s1,4 */
    a1 = 2u;                                        /* addiu a1,zero,2: bne delay slot, runs on both paths */
    if (v0 != 0u) goto L66C0;                       /* bne v0,zero,66c0 */
    return v0;                                      /* j 6734 with $v0 = 0 */

    /* 6714: v1 is none of 0..3 */
L6714:
    v1 = S8_(RD8(p + 0x4Du));                       /* lb v1,0x4d(s0) */
    v0 = 0xFFFFFFFFu;                               /* addiu v0,zero,-1 */
    if (v1 != v0) goto L672C;                       /* bne v1,v0,672c */
    a0 = p;                                         /* addu a0,s0,zero: delay slot of jal 800af7ec */
    v0 = m->call4(m->ctx, 0x800AF7ECu, a0, a1, a2, a3);
    REFRESH();

L672C:
    a0 = p;                                         /* addu a0,s0,zero: delay slot of jal 80026a04 */
    v0 = m->call4(m->ctx, 0x80026A04u, a0, a1, a2, a3);
    return v0;                                      /* epilogue returns the callee's $v0 */

#undef REFRESH
}
