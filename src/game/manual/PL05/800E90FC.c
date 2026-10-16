#include "manual.h"
#include "hex2c_rt.h"

/* 800E90FC (PL05, slot A, 1772 bytes). $a0 = P (struct, kept in s2). $a1, $a2 and $a3 are never written before a jal,
 * and every path passes them on to the 80025bf8 / 80025548 callees, so nargs is 4. The callees may change $a0..$a3, so
 * after every call they are reloaded with gpr_get (as VS_SEL 800DFC48 does).
 * Jump table: lbu v0,0x1fa(s0); idx = v0 - 1; if idx < 15, jr through the word at 0x800DF204 + 4*idx (the `lui 800e;
 * addiu -3580` pair, not a load of the table base). The branch delay slots are run on both paths, and where a delay
 * slot writes a register the branch tests (bne at 9138 with addiu v0,v1,-1; bne at 95b4 with addiu v0,zero,2) the
 * branch reads the value from before the slot. Every exit leaves its own $v0 (the jump target 0x800E97C4 for the table
 * entries that go straight to the epilogue). Generated C mismatched with v0 = 0x800E0000 on that path. */

#define RNG_FN   0x80025BF8u   /* jal 80025bf8: no arguments set by this function */
#define NEW_FN   0x80025548u   /* jal 80025548: object allocation, $a0..$a2 set before the jal */
#define JTBL     0x800DF204u   /* jump table, 15 words */

#define REFRESH() do {                                  \
        a0 = m->gpr_get(m->ctx, 4u);                    \
        a1 = m->gpr_get(m->ctx, 5u);                    \
        a2 = m->gpr_get(m->ctx, 6u);                    \
        a3 = m->gpr_get(m->ctx, 7u);                    \
    } while (0)

#define CALL_RNG() do {                                 \
        v0 = m->call4(m->ctx, RNG_FN, a0, a1, a2, a3);  \
        REFRESH();                                      \
    } while (0)

#define S8(x)  ((uint32_t)(int32_t)(int8_t)(x))
#define S16(x) ((uint32_t)(int32_t)(int16_t)(x))

uint32_t man_PL05_800E90FC(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;                        /* s2 */
    uint32_t s0, s1, s3, s4, s5;
    uint32_t v0, v1, t;

    /* 800e9120..9134 */
    s0 = RD32(RD32(p + 0x80u) + 0x178u);    /* lw v0,0x80(s2); lw s0,0x178(v0) */
    v1 = RD16(p + 0x0Cu);                   /* lhu v1,0xc(s2) */
    t = S16(RD16(p + 0x0Cu));               /* lh v0,0xc(s2) (tested by bne) */
    v0 = v1 - 1u;                           /* bne delay slot addiu v0,v1,-1: runs on both paths */
    if (t != 0u) goto L97C0;

    /* 800e913c: switch on lbu 0x1fa(s0) - 1 */
    v0 = RD8(s0 + 0x1FAu);
    v1 = v0 - 1u;
    v0 = (v1 < 15u);                        /* sltiu v0,v1,15 */
    if (v0 == 0u) goto L97C4;
    v0 = RD32(JTBL + 4u * v1);              /* sll v1,v1,2; addu v1,v1,v0; lw v0,0(v1); jr v0 */
    switch (v0) {
    case 0x800E920Cu: goto L920C;
    case 0x800E97C4u: goto L97C4;
    case 0x800E93A8u: goto L93A8;
    case 0x800E9528u: goto L9528;
    case 0x800E95C4u: goto L95C4;
    case 0x800E96D4u: goto L96D4;
    case 0x800E9170u: goto L9170;
    default:          goto L97C4;           /* not reached: all table words are listed above */
    }

    /* 800e9170: loop with the 0x6a/0x6e offsets (jump-table entry 14) */
L9170:
    s1 = 0u;
L9174:
    CALL_RNG();
    v0 &= 7u;
    v0 = (s1 < v0);                         /* slt v0,s1,v0 */
    if (v0 == 0u) goto L91F8;
    v0 = RD8(s0 + 0x0Bu);
    if (v0 == 0u) goto L91B8;
    CALL_RNG();                             /* jal 80025bf8 at 919c */
    v0 &= 0x7Fu;
    v1 = RD16(s0 + 0x6Au);
    v0 = v0 - 48u;
    v1 = v1 - v0;                           /* j 91d4; subu delay */
    goto L91D4;
L91B8:
    CALL_RNG();
    v0 &= 0x7Fu;
    a0 = 0xFFD0u;                           /* ori a0,zero,0xffd0 (clobbers $a0) */
    v1 = RD16(s0 + 0x6Au);
    v0 = v0 + a0;
    v1 = v1 + v0;
L91D4:
    WR16(p + 0x72u, v1);                    /* sh v1,0x72(s2): delay slot of jal 80025bf8 at 91d4 */
    CALL_RNG();
    s1 = s1 + 1u;                           /* addiu s1,s1,1 */
    v1 = RD16(s0 + 0x6Eu);
    v0 &= 0xFu;
    v1 = v1 + v0;
    v1 = v1 - 16u;
    WR16(p + 0x76u, v1);                    /* j 9174; sh v1,0x76(s2) in the delay slot */
    goto L9174;

    /* 800e91f8 */
L91F8:
    CALL_RNG();
    v0 = (v0 & 7u) + 2u;
    goto L97C0;

    /* 800e920c (jump-table entries 0 and 6) */
L920C:
    v0 = S16(RD16(s0 + 0x6Eu));             /* lh v0,0x6e(s0) */
    v0 = ((int32_t)v0 < 36);                /* slti v0,v0,36 */
    s1 = 0u;                                /* addu s1,zero,zero: bne delay slot, runs on both paths */
    if (v0 != 0u) goto L92AC;
L9220:
    CALL_RNG();
    v0 &= 7u;
    v0 = (s1 < v0);
    if (v0 == 0u) goto L9298;
    v0 = RD8(s0 + 0x0Bu);
    if (v0 == 0u) goto L9264;
    CALL_RNG();
    v0 &= 0x7Fu;
    v1 = RD16(s0 + 0x6Au);
    v0 = v0 - 48u;
    v1 = v1 - v0;                           /* j 9280; subu delay */
    goto L9280;
L9264:
    CALL_RNG();
    v0 &= 0x7Fu;
    a0 = 0xFFD0u;
    v1 = RD16(s0 + 0x6Au);
    v0 = v0 + a0;
    v1 = v1 + v0;
L9280:
    WR16(p + 0x72u, v1);
    v0 = RD16(s0 + 0x6Eu);
    s1 = s1 + 1u;
    v0 = v0 + 48u;
    WR16(p + 0x76u, v0);                    /* j 9220; sh v0,0x76(s2) in the delay slot */
    goto L9220;
    /* 800e9298 */
L9298:
    CALL_RNG();
    v0 = (v0 & 7u) + 2u;
    goto L97C0;

    /* 800e92ac */
L92AC:
    CALL_RNG();
    v0 &= 3u;
    v0 = (s1 < v0);
    if (v0 == 0u) goto L9398;
    CALL_RNG();
    v0 &= 0x7Fu;
    a0 = 0xFFC0u;
    v1 = RD16(s0 + 0x6Au);
    v0 = v0 + a0;
    v1 = v1 + v0;
    WR16(p + 0x72u, v1);
    v0 = RD16(s0 + 0x6Eu);
    a1 = 1u;                                /* addiu a1,zero,1 */
    WR16(p + 0x76u, v0);
    a0 = (RD8(s0 + 0x136u) + 4u) & 0xFFu;
    a2 = 9u;
    v0 = m->call4(m->ctx, NEW_FN, a0, a1, a2, a3);   /* jal 80025548 */
    REFRESH();
    v1 = v0;
    if (v1 == 0u) goto L933C;
    WR32(v1 + 0x80u, s0);
    WR8(v1 + 4u, 0u);
    WR8(v1 + 0x0Bu, RD8(p + 0x0Bu));
    WR16(v1 + 0x6Au, RD16(p + 0x72u));
    WR16(v1 + 0x6Eu, RD16(p + 0x76u));
L933C:
    v0 = RD8(s0 + 0x0Bu);
    if (v0 == 0u) goto L9368;
    CALL_RNG();
    v0 &= 0x7Fu;
    v1 = RD16(s0 + 0x6Au);
    v0 = v0 - 48u;
    v1 = v1 - v0;                           /* j 9384; subu delay */
    goto L9384;
L9368:
    CALL_RNG();
    v0 &= 0x7Fu;
    a0 = 0xFFD0u;
    v1 = RD16(s0 + 0x6Au);
    v0 = v0 + a0;
    v1 = v1 + v0;
L9384:
    WR16(p + 0x72u, v1);
    v0 = RD16(s0 + 0x6Eu);
    s1 = s1 + 1u;
    WR16(p + 0x76u, v0);                    /* j 92ac; sh v0,0x76(s2) in the delay slot */
    goto L92AC;
    /* 800e9398 */
L9398:
    CALL_RNG();
    v0 = v0 & 7u;
    goto L97C0;

    /* 800e93a8 (jump-table entry 3) */
L93A8:
    v1 = RD8(s0 + 0x21Bu);
    v0 = 2u;
    s1 = 0u;                                /* addu s1,zero,zero: bne delay slot, runs on both paths */
    if (v1 != v0) goto L9444;
L93B8:
    CALL_RNG();
    v0 &= 7u;
    v0 = (s1 < v0);
    if (v0 == 0u) goto L9430;
    v0 = RD8(s0 + 0x0Bu);
    if (v0 == 0u) goto L93FC;
    CALL_RNG();
    v0 &= 0x70u;
    v1 = RD16(s0 + 0x6Au);
    v0 = v0 - 48u;
    v1 = v1 - v0;                           /* j 9418; subu delay */
    goto L9418;
L93FC:
    CALL_RNG();
    v0 &= 0x70u;
    a0 = 0xFFD0u;
    v1 = RD16(s0 + 0x6Au);
    v0 = v0 + a0;
    v1 = v1 + v0;
L9418:
    WR16(p + 0x72u, v1);
    v0 = RD16(s0 + 0x6Eu);
    s1 = s1 + 1u;
    v0 = v0 + 40u;
    WR16(p + 0x76u, v0);                    /* j 93b8; sh v0,0x76(s2) in the delay slot */
    goto L93B8;
    /* 800e9430 */
L9430:
    CALL_RNG();
    v0 = (v0 & 7u) + 4u;
    goto L97C0;

    /* 800e9444 */
L9444:
    s4 = 1u;
    s5 = 0x800E9E70u;                       /* lui v0,0x800f; addiu s5,v0,-24976 */
    s3 = s5;                                /* addu s3,s5,zero */
L9454:
    v0 = RD8(s0 + 0x0Bu);
    if (v0 != 0u) goto L947C;
    v0 = S8(RD8(s3));
    v1 = RD16(s0 + 0x6Au) + v0;
    goto L9490;
L947C:
    v0 = S8(RD8(s3));
    v1 = RD16(s0 + 0x6Au) - v0;
L9490:
    WR16(p + 0x72u, v1);
    v0 = s4 + s5;                           /* addu v0,s4,s5 */
    a1 = 1u;                                /* addiu a1,zero,1 */
    v1 = S8(RD8(v0));
    v0 = RD16(s0 + 0x6Eu) + v1;
    WR16(p + 0x76u, v0);
    a0 = (RD8(s0 + 0x136u) + 4u) & 0xFFu;
    a2 = 9u;
    v0 = m->call4(m->ctx, NEW_FN, a0, a1, a2, a3);   /* jal 80025548 at 94c0 */
    REFRESH();
    v1 = v0;
    v0 = s1 & 1u;                           /* andi v0,s1,1: beq delay slot, runs on both paths */
    if (v1 == 0u) goto L9500;
    WR32(v1 + 0x80u, s0);
    WR8(v1 + 4u, v0);
    WR8(v1 + 0x0Bu, RD8(p + 0x0Bu));
    WR16(v1 + 0x6Au, RD16(p + 0x72u));
    WR16(v1 + 0x6Eu, RD16(p + 0x76u));
L9500:
    s4 = s4 + 2u;
    s1 = s1 + 1u;
    v0 = (s1 < 4u);                         /* slti v0,s1,4 */
    s3 = s3 + 2u;                           /* bne delay slot, runs on both paths */
    if (v0 != 0u) goto L9454;
    CALL_RNG();
    v0 = (v0 & 7u) + 4u;
    goto L97C0;

    /* 800e9528 (jump-table entry 4) */
L9528:
    WR16(p + 0x6Eu, 35u);                   /* addiu v0,zero,35; sh v0,0x6e(s2) */
    s1 = 0u;
L9534:
    CALL_RNG();
    v0 &= 0x7Fu;
    a0 = 0xFFC0u;
    v1 = RD16(s0 + 0x6Au);
    v0 = v0 + a0;
    v1 = v1 + v0;
    WR16(p + 0x72u, v1);
    v0 = RD16(s0 + 0x6Eu);
    a1 = 1u;
    WR16(p + 0x76u, v0);
    a0 = (RD8(s0 + 0x136u) + 4u) & 0xFFu;
    a2 = 9u;
    v0 = m->call4(m->ctx, NEW_FN, a0, a1, a2, a3);
    REFRESH();
    v1 = v0;
    if (v1 == 0u) goto L95AC;
    WR32(v1 + 0x80u, s0);
    WR8(v1 + 4u, 0u);
    WR8(v1 + 0x0Bu, RD8(p + 0x0Bu));
    WR16(v1 + 0x6Au, RD16(p + 0x72u));
    WR16(v1 + 0x6Eu, RD16(p + 0x76u));
L95AC:
    s1 = s1 + 1u;
    t = (s1 < 4u);                          /* slti v0,s1,4; the bne reads this, not the delay slot */
    v0 = 2u;                                /* addiu v0,zero,2: bne delay slot */
    if (t != 0u) goto L9534;
    WR16(p + 0x0Cu, v0);                    /* j 97c4; sh v0,0xc(s2) in the delay slot */
    goto L97C4;

    /* 800e95c4 (jump-table entry 7) */
L95C4:
    v0 = RD8(s0 + 0x206u);
    v0 = v0 - 2u;
    v0 = (v0 < 2u);                         /* sltiu v0,v0,2 */
    if (v0 == 0u) goto L97C4;
    v0 = S16(RD16(s0 + 0x6Eu));
    v0 = ((int32_t)v0 < 36);                /* slti v0,v0,36 */
    s1 = 2u;                                /* addiu s1,zero,2: bne delay slot, runs on both paths */
    if (v0 != 0u) goto L97C4;
    s4 = 5u;
    s5 = 0x800E9E70u;                       /* lui v0,0x800f; addiu s5,v0,-24976 */
    s3 = s5 + 4u;                           /* addiu s3,s5,4 */
L9600:
    v0 = RD8(s0 + 0x0Bu);
    if (v0 != 0u) goto L9628;
    v0 = S8(RD8(s3));
    v1 = RD16(s0 + 0x6Au);
    v1 = v1 + v0;                           /* j 963c; addu delay */
    goto L963C;
L9628:
    v0 = S8(RD8(s3));
    v1 = RD16(s0 + 0x6Au);
    v1 = v1 - v0;
L963C:
    WR16(p + 0x72u, v1);
    v0 = s4 + s5;
    a1 = 1u;
    v1 = S8(RD8(v0));
    v0 = RD16(s0 + 0x6Eu) + v1;
    WR16(p + 0x76u, v0);
    a0 = (RD8(s0 + 0x136u) + 4u) & 0xFFu;
    a2 = 9u;
    v0 = m->call4(m->ctx, NEW_FN, a0, a1, a2, a3);
    REFRESH();
    v1 = v0;
    v0 = s1 & 1u;                           /* andi v0,s1,1: beq delay slot */
    if (v1 == 0u) goto L96AC;
    WR32(v1 + 0x80u, s0);
    WR8(v1 + 4u, v0);
    WR8(v1 + 0x0Bu, RD8(p + 0x0Bu));
    WR16(v1 + 0x6Au, RD16(p + 0x72u));
    WR16(v1 + 0x6Eu, RD16(p + 0x76u));
L96AC:
    s4 = s4 + 2u;
    s1 = s1 + 1u;
    v0 = (s1 < 4u);
    s3 = s3 + 2u;                           /* bne delay slot */
    if (v0 != 0u) goto L9600;
    CALL_RNG();
    v0 = (v0 & 3u) + 2u;
    goto L97C0;

    /* 800e96d4 (jump-table entry 11) */
L96D4:
    s5 = 1u;
    s3 = s5;                                /* addu s3,s5,zero */
    s4 = 0x800E9E78u;                       /* lui v0,0x800f; addiu s4,v0,-24968 */
    s1 = s4;                                /* addu s1,s4,zero */
L96E8:
    v0 = RD8(s0 + 0x0Bu);
    if (v0 != 0u) goto L9710;
    v0 = S8(RD8(s1));
    v1 = RD16(s0 + 0x6Au) + v0;
    goto L9724;
L9710:
    v0 = S8(RD8(s1));
    v1 = RD16(s0 + 0x6Au) - v0;
L9724:
    WR16(p + 0x72u, v1);
    v0 = s3 + s4;                           /* addu v0,s3,s4 */
    v1 = RD16(s0 + 0x6Eu) + S8(RD8(v0));    /* lbu v0,0(v0); lhu v1,0x6e(s0); sign-extend; addu */
    WR16(p + 0x76u, v1);
    v0 = RD8(s0 + 0x1FCu);
    a1 = 1u;                                /* addiu a1,zero,1: bne delay slot */
    if (v0 != s5) goto L975C;
    v0 = v1 - 24u;                          /* addiu v0,v1,-24 */
    WR16(p + 0x76u, v0);
L975C:
    a0 = (RD8(s0 + 0x136u) + 4u) & 0xFFu;
    a2 = 9u;
    v0 = m->call4(m->ctx, NEW_FN, a0, a1, a2, a3);
    REFRESH();
    v1 = v0;
    if (v1 == 0u) goto L97A8;
    WR32(v1 + 0x80u, s0);
    WR8(v1 + 4u, s5);
    WR8(v1 + 0x0Bu, RD8(p + 0x0Bu));
    WR16(v1 + 0x6Au, RD16(p + 0x72u));
    WR16(v1 + 0x6Eu, RD16(p + 0x76u));
L97A8:
    s1 = s1 + 2u;
    v0 = s4 + 6u;
    t = (s1 < v0);                          /* slt v0,s1,v0 */
    s3 = s3 + 2u;                           /* bne delay slot */
    if (t != 0u) goto L96E8;
    v0 = 20u;
    /* falls into 800e97c0 */

L97C0:
    WR16(p + 0x0Cu, v0);                    /* sh v0,0xc(s2) */
    /* falls into 800e97c4 */
L97C4:
    return v0;
}
