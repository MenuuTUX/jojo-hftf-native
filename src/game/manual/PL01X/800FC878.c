#include "manual.h"
#include "hex2c_rt.h"

/* PL01X 800FC878..800FCE10 (1432 bytes, slot B, base 800F4800). $a0 = P (the character). Returns $v0.
 * Written from the MIPS (extracted/M/PL01X.BIN at offset 0x8078), not from Hex-Rays. Each label is the MIPS address of its
 * block. Delay slots run where the MIPS runs them (on both paths of a branch).
 * Calls: r1..r3 hold $a1..$a3 as the next jal sees them. They start as the entry values and are reloaded with
 * m->exit_reg right after every call (keep_leftovers), so a callee's leftover $a2/$a3 (800258F0, 800268BC) reaches the next
 * call. $a0 is set to P before every jal in the MIPS, so it is passed as P.
 * The state switch is the jump table at 0x800F48F4: 0 800fc934, 1 800fc97c, 2 800fca94, 3 800fcb10, 4 800fcbc0, 5 800fcc0c,
 * 6 and 7 and the rest default, 8 800fcc58, 9 800fcc78, 10 800fccc0, 11 800fccd8, 12 800fccf8, 13 800fcd34, 14 800fcd54.
 * The prologue lowers $sp by 40 as the MIPS does. The s0..s3/ra spills are not written (only this function reads them back). */
static void keep_leftovers(const GuestMem *m, uint32_t *r1, uint32_t *r2, uint32_t *r3) {
    *r1 = m->exit_reg(m->ctx, 5u);
    *r2 = m->exit_reg(m->ctx, 6u);
    *r3 = m->exit_reg(m->ctx, 7u);
}

uint32_t man_PL01X_800FC878(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;                                   /* $s0 */
    uint32_t q, s2, s3;                                /* $s1, $s2, $s3 */
    uint32_t v0, v1, t;
    int32_t hw, sv;                                    /* signed halfword / byte loads */
    int c;
    uint32_t r1 = a1, r2 = a2, r3 = a3;                /* $a1..$a3 as the next jal sees them */
    uint32_t sp0 = m->sp_get(m->ctx) - 40u;

    m->sp_set(m->ctx, sp0);
    q = RD32(p + 0x80u);                               /* 800fc898 lw s1,0x80(s0) */
    v0 = RD8(0x80066CA2u);                             /* 800fc89c lbu v0,0x6ca2(v0) */
    s2 = RD32(q + 0x174u);                             /* 800fc8a0 lw s2,0x174(s1) */
    s3 = RD32(q + 0x17Cu);                             /* 800fc8a4 lw s3,0x17c(s1) */
    if (v0 == 0u) goto state;                          /* 800fc8a8 beq v0,zero,800fc900 */

    /* 800fc8ac..800fc8ec: the byte at 0x80066CA2 is not zero. */
    t = RD32(p + 0x108u);                              /* 800fc8b0 lw v0,0x108(s0) */
    v0 = 1u;                                           /* 800fc8bc delay slot, both paths */
    if (t == 0u) goto out;                             /* 800fc8b8 beq v0,zero,800fcdf4 */
    v1 = RD8(p + 4u);                                  /* 800fc8c0 lbu v1,4(s0) */
    v0 = 1u << (v1 & 31u);                             /* 800fc8c8 sllv v0,v0,v1 */
    v1 = RD8(q + 0x191u);                              /* 800fc8cc lbu v1,0x191(s1) */
    v0 = ~v0;                                          /* 800fc8d0 nor v0,zero,v0 */
    v1 &= v0;                                          /* 800fc8d4 and v1,v1,v0 */
    WR8(q + 0x191u, v1);                               /* 800fc8d8 sb v1,0x191(s1) */
    v0 = RD8(p + 5u);                                  /* 800fc8dc lbu v0,5(s0) */
    v1 = RD8(p + 0x17u);                               /* 800fc8e0 lbu v1,0x17(s0) */
    v0 += 1u;                                          /* 800fc8e4 addiu v0,v0,1 */
    WR8(p + 5u, v0);                                   /* 800fc8ec sb v0,5(s0) (delay slot of the beq below) */
    if (v1 == 0u) goto out;                            /* 800fc8e8 beq v1,zero,800fcdf4 */
    v0 = m->call4(m->ctx, 0x80025A50u, p, 2u, r2, r3); /* 800fc8f0 jal 80025a50 (delay: a1 = 2) */
    keep_leftovers(m, &r1, &r2, &r3);
    goto out;

state:
    /* 800fc900: dispatch on the state byte P+6. */
    v0 = RD16(q + 0x1Eu);                              /* 800fc900 lhu v0,0x1e(s1) */
    v1 = RD8(p + 6u);                                  /* 800fc904 lbu v1,6(s0) */
    WR16(p + 0x1Eu, v0);                               /* 800fc908 sh v0,0x1e(s0) */
    if (!(v1 < 15u)) goto dflt;                        /* 800fc90c sltiu; 800fc910 beq v0,zero,800fcdc8 */
    switch (v1) {
    case 0: goto case0;
    case 1: goto label9;
    case 2: goto case2;
    case 3: goto case3;
    case 4: goto case4;
    case 5: goto case5;
    case 8: goto case8;
    case 9: goto case9;
    case 10: goto case10;
    case 11: goto case11;
    case 12: goto case12;
    case 13: goto case13;
    case 14: goto case14;
    default: goto dflt;
    }

case0:
    v0 = RD8(p + 6u) + 1u;                             /* 800fc934 lbu v0,6(s0); 800fc93c addiu */
    WR8(p + 6u, v0);                                   /* 800fc940 sb v0,6(s0) */
    v0 = RD8(s2 + 0x1FDu);                             /* 800fc944 lbu v0,0x1fd(s2) */
    v1 = (v0 != 0u) ? 600u : 1600u;                    /* 800fc94c beq (delay addiu v3,1600); 800fc954 addiu v3,600 */
    t = RD8(p + 4u);                                   /* 800fc958 lbu a1,4(s0) */
    WR16(p + 0xCu, v1);                                /* 800fc960 sh v3,0xc(s0) */
    r1 = (t + 41u) & 0xFFu;                            /* 800fc964 addiu a1,a1,41; 800fc96c andi a1,a1,0xff */
    v0 = m->call4(m->ctx, 0x800BB2A4u, p, r1, r2, r3); /* 800fc968 jal 800bb2a4 */
    keep_leftovers(m, &r1, &r2, &r3);
    v0 = m->call4(m->ctx, 0x800258F0u, p, 2u, r2, r3); /* 800fc974 jal 800258f0 (delay: a1 = 2) */
    keep_leftovers(m, &r1, &r2, &r3);

label9:
    /* 800fc97c: state 1, and the fall-through of state 0. */
    v1 = RD32(s2 + 4u) & 0x00FFFF00u;                 /* 800fc97c..800fc988 lui, lw, ori, and */
    v0 = 11u;                                          /* 800fc994 delay slot, both paths */
    if (v1 != 0x100u) goto to11;                       /* 800fc990 bne v1,v0,800fcb94 */
    hw = RDS16(0x80066CEAu);                           /* 800fc998 lui v0,0x8006; 800fc99c lh a0,0x6cea(v0) */
    v1 = (uint32_t)RDS16(p + 0x6Au);                   /* 800fc9a0 lh v1,0x6a(s0) */
    c = (hw + 480 < (int32_t)v1);                      /* 800fc9a4 addiu v0,a0,480; 800fc9a8 slt v0,v0,v1 */
    v0 = 11u;                                          /* 800fc9ac delay */
    if (c) goto to11;                                  /* 800fc9ac bne v0,zero,800fcb94 */
    c = ((int32_t)v1 < hw - 96);                       /* 800fc9b4 addiu v0,a0,-96; 800fc9b8 slt v0,v1,v0 */
    v0 = 11u;                                          /* 800fc9bc delay */
    if (c) goto to11;                                  /* 800fc9bc bne v0,zero,800fcb94 */
    t = RD32(p + 0x108u);                              /* 800fc9c4 lw v0,0x108(s0) */
    v0 = 13u;                                          /* 800fc9cc delay */
    if (t != 0u) goto to13;                            /* 800fc9cc bne v0,zero,800fcbac */
    v0 = (uint32_t)RDS16(p + 0xCu);                    /* 800fc9d4 lh v0,0xc(s0) */
    if ((int32_t)v0 == 0) goto ca48;                   /* 800fc9dc beq v0,zero,800fca48 */
    if (RD8(s2 + 0x1FDu) != 0u) goto ca24;             /* 800fc9e4 lbu v0,0x1fd(s2); 800fc9ec bne v0,zero,800fca24 */
    v1 = RD8(p + 4u);                                  /* 800fc9f4 lbu v1,4(s0) (800fc9f0 lui v0,0x8010 in delay) */
    v0 = 0x800FF910u;                                  /* 800fc9f8 addiu v0,v0,-1776 */
    v1 = (v1 << 1) + v0;                               /* 800fc9fc sll v1,v1,1; 800fca00 addu v1,v1,v0 */
    v0 = RD16(q + 0x1CCu);                             /* 800fca04 lhu v0,0x1cc(s1) */
    v1 = RD16(v1);                                     /* 800fca08 lhu v1,0(v1) */
    if ((v0 & v1) != 0u) goto ca80;                    /* 800fca10 and v0,v0,v1; 800fca14 beq v0,zero,800fca48 */
    goto ca48;

ca24:
    /* 800fca24: |P+0x6A - S+0x6A| < 384 selects the ca48 block. */
    v0 = (uint32_t)RDS16(s3 + 0x6Au) - v1;             /* 800fca24 lh v0,0x6a(s3); 800fca2c subu v0,v0,v1 */
    if ((int32_t)v0 < 0) v0 = 0u - v0;                 /* 800fca30 bgez; 800fca38 subu v0,zero,v0 */
    c = ((int32_t)v0 < 384);                           /* 800fca3c slti v0,v0,384 */
    if (!c) goto ca80;                                 /* 800fca40 beq v0,zero,800fca80 */

ca48:
    v0 = RD8(p + 6u);                                  /* 800fca48 lbu v0,6(s0) */
    v1 = 1u;                                           /* 800fca4c addiu v1,zero,1 */
    WR8(p + 1u, v1);                                   /* 800fca50 sb v1,1(s0) */
    WR16(p + 0xF0u, 0u);                               /* 800fca54 sh zero,0xf0(s0) */
    WR16(p + 0xEEu, 0u);                               /* 800fca58 sh zero,0xee(s0) */
    v0 += 1u;                                          /* 800fca5c addiu v0,v0,1 */
    WR8(p + 6u, v0);                                   /* 800fca60 sb v0,6(s0) */
    r1 = RD32(0x8010347Cu);                            /* 800fca64 lui v0,0x8010; 800fca68 lw a1,0x347c(v0) */
    v0 = m->call4(m->ctx, 0x800268BCu, p, r1, r2, r3); /* 800fca6c jal 800268bc (a2/a3: leftovers of 800258f0 on this path) */
    keep_leftovers(m, &r1, &r2, &r3);
    v0 = m->call4(m->ctx, 0x80024104u, p, 299u, r2, r3); /* 800fca78 jal 80024104 (delay: a1 = 299) */
    keep_leftovers(m, &r1, &r2, &r3);

ca80:
    v0 = RD16(p + 0xCu) - 1u;                          /* 800fca80 lhu v0,0xc(s0); 800fca88 addiu v0,v0,-1 */
    WR16(p + 0xCu, v0);                                /* 800fca8c sh v0,0xc(s0) */
    goto out;

case2:
    /* 800fca94: state 2. */
    v1 = RD32(s2 + 4u) & 0x00FFFF00u;                  /* 800fca94..800fcaa0 */
    v0 = 11u;                                          /* 800fcaa8 delay */
    if (v1 != 0x100u) goto to11;                       /* 800fcaa8 bne v1,v0,800fcb94 */
    v0 = RD16(p + 0xF0u) + 10u;                        /* 800fcab0 lhu v0,0xf0(s0); 800fcab8 addiu v0,v0,10 */
    WR16(p + 0xF0u, v0);                               /* 800fcabc sh v0,0xf0(s0) */
    WR16(p + 0xEEu, v0);                               /* 800fcac0 sh v0,0xee(s0) */
    v0 &= 0xFFFFu;                                     /* 800fcac4 andi v0,v0,0xffff */
    c = (v0 < 126u);                                   /* 800fcac8 sltiu v0,v0,126 */
    v1 = 126u;                                         /* 800fcacc delay (addiu v3,zero,126) */
    if (c) goto cb00;                                  /* 800fcacc bne v0,zero,800fcb00 */
    v0 = RD8(p + 6u);                                  /* 800fcad4 lbu v0,6(s0) */
    WR16(p + 0xF0u, v1);                               /* 800fcad8 sh v3,0xf0(s0) */
    WR16(p + 0xEEu, v1);                               /* 800fcadc sh v3,0xee(s0) */
    v0 += 1u;                                          /* 800fcae0 addiu v0,v0,1 */
    WR8(p + 6u, v0);                                   /* 800fcae4 sb v0,6(s0) */
    r1 = RD32(0x80103490u);                            /* 800fcae8 lui v0,0x8010; 800fcaec lw a1,0x3490(v0) */
    v0 = m->call4(m->ctx, 0x800268BCu, p, r1, r2, r3); /* 800fcaf0 jal 800268bc (delay: addu a0,s0) */
    keep_leftovers(m, &r1, &r2, &r3);
    v0 = 30u;                                          /* 800fcaf8 addiu v0,zero,30 */
    WR16(p + 0xCu, v0);                                /* 800fcafc sh v0,0xc(s0) */

cb00:
    /* 800fcb00: the join. On the taken branch, r1..r3 are the entry values. */
    v0 = m->call4(m->ctx, 0x80026A04u, p, r1, r2, r3); /* 800fcb00 jal 80026a04 */
    keep_leftovers(m, &r1, &r2, &r3);
    goto out;

case3:
    /* 800fcb10: state 3. */
    v0 = RD8(p + 0x139u);                              /* 800fcb10 lbu v0,0x139(s0) */
    if (v0 == 0u) goto cb3c;                           /* 800fcb18 beq v0,zero,800fcb3c */
    v0 = RD8(p + 6u);                                  /* 800fcb20 lbu v0,6(s0) */
    r1 = 300u;                                         /* 800fcb24 addiu a1,zero,300 */
    v0 += 1u;                                          /* 800fcb28 addiu v0,v0,1 */
    WR8(p + 6u, v0);                                   /* 800fcb30 sb v0,6(s0) (delay slot, before the callee) */
    v0 = m->call4(m->ctx, 0x80024104u, p, r1, r2, r3); /* 800fcb2c jal 80024104 */
    keep_leftovers(m, &r1, &r2, &r3);
    WR16(p + 0x100u, 0u);                              /* 800fcb38 sh zero,0x100(s0) */
    goto out;

cb3c:
    v0 = (uint32_t)RDS16(p + 0xCu);                    /* 800fcb3c lh v0,0xc(s0) */
    r1 = RD16(p + 0xCu);                               /* 800fcb40 lhu a1,0xc(s0) */
    if ((int32_t)v0 == 0) goto cb90;                   /* 800fcb44 beq v0,zero,800fcb90 */
    v1 = RD32(s2 + 4u) & 0x00FFFF00u;                  /* 800fcb48..800fcb54 */
    v0 = 11u;                                          /* 800fcb60 delay */
    if (v1 != 0x100u) goto to11;                       /* 800fcb5c bne v1,v0,800fcb94 */
    hw = RDS16(0x80066CEAu);                           /* 800fcb64 lui v0,0x8006; 800fcb68 lh a0,0x6cea(v0) */
    v1 = (uint32_t)RDS16(p + 0x6Au);                   /* 800fcb6c lh v1,0x6a(s0) */
    c = (hw + 480 < (int32_t)v1);                      /* 800fcb70 addiu; 800fcb74 slt */
    v0 = 11u;                                          /* 800fcb7c delay */
    if (c) goto to11;                                  /* 800fcb78 bne v0,zero,800fcb94 */
    c = ((int32_t)v1 < hw - 96);                       /* 800fcb80 addiu; 800fcb84 slt */
    if (!c) goto cb9c;                                 /* 800fcb88 beq v0,zero,800fcb9c */
cb90:
    v0 = 11u;                                          /* 800fcb90 addiu v0,zero,11 */
to11:
    WR8(p + 6u, v0);                                   /* 800fcb94 j 800fcdf4; sb v0,6(s0) */
    goto out;
cb9c:
    t = RD32(p + 0x108u);                              /* 800fcb9c lw v0,0x108(s0) */
    if (t == 0u) goto cbb4;                            /* 800fcba4 beq v0,zero,800fcbb4 (delay: v0 = 13) */
    v0 = 13u;                                          /* 800fcba8 addiu v0,zero,13 */
to13:
    WR8(p + 6u, v0);                                   /* 800fcbac j 800fcdf4; sb v0,6(s0) */
    goto out;
cbb4:
    v0 = r1 - 1u;                                      /* 800fcbb4 addiu v0,a1,-1 */
    WR16(p + 0xCu, v0);                                /* 800fcbbc sh v0,0xc(s0) */
    goto out;

case4:
    /* 800fcbc0: state 4. */
    v0 = RD16(p + 0xEEu);                              /* 800fcbc0 lhu v0,0xee(s0) */
    c = (v0 < 254u);                                   /* 800fcbc8 sltiu v0,v0,254 */
    if (!c) goto cc00;                                 /* 800fcbcc beq v0,zero,800fcc00 */
    v0 = RD16(p + 0xF0u) + 16u;                        /* 800fcbd4 lhu v0,0xf0(s0); 800fcbdc addiu v0,v0,16 */
    WR16(p + 0xF0u, v0);                               /* 800fcbe0 sh v0,0xf0(s0) */
    WR16(p + 0xEEu, v0);                               /* 800fcbe4 sh v0,0xee(s0) */
    v0 &= 0xFFFFu;                                     /* 800fcbe8 andi v0,v0,0xffff */
    c = (v0 < 255u);                                   /* 800fcbec sltiu v0,v0,255 */
    v0 = 254u;                                         /* 800fcbf4 delay */
    if (c) goto out;                                   /* 800fcbf0 bne v0,zero,800fcdf4 */
    WR16(p + 0xF0u, v0);                               /* 800fcbfc sh v0,0xf0(s0) */
    WR16(p + 0xEEu, v0);                               /* 800fcc4c sh v0,0xee(s0) */
    goto out;
cc00:
    v0 = RD8(p + 6u) + 1u;                             /* 800fcc00 lbu v0,6(s0); 800fcc04 addiu v0,v0,1 */
cc98:
    WR8(p + 6u, v0);                                   /* 800fcc9c sb v0,6(s0) (after the j in 800fcc98) */
    goto out;

case5:
    /* 800fcc0c: state 5. */
    v0 = RD16(p + 0xEEu);                              /* 800fcc0c lhu v0,0xee(s0) */
    c = (v0 < 223u);                                   /* 800fcc14 sltiu v0,v0,223 */
    v0 = 8u;                                           /* 800fcc1c delay */
    if (c) {                                           /* 800fcc18 bne v0,zero,800fcc50 */
        WR8(p + 6u, v0);                               /* 800fcc50 j 800fcdf4; sb v0,6(s0) */
        goto out;
    }
    v0 = RD16(p + 0xF0u) - 8u;                         /* 800fcc20 lhu v0,0xf0(s0); 800fcc28 addiu v0,v0,-8 */
    WR16(p + 0xF0u, v0);                               /* 800fcc2c sh v0,0xf0(s0) */
    WR16(p + 0xEEu, v0);                               /* 800fcc30 sh v0,0xee(s0) */
    v0 &= 0xFFFFu;                                     /* 800fcc34 andi v0,v0,0xffff */
    c = (v0 < 222u);                                   /* 800fcc38 sltiu v0,v0,222 */
    v0 = 222u;                                         /* 800fcc40 delay */
    if (!c) goto out;                                  /* 800fcc3c beq v0,zero,800fcdf4 */
    WR16(p + 0xF0u, v0);                               /* 800fcc44 sh v0,0xf0(s0) */
    WR16(p + 0xEEu, v0);                               /* 800fcc4c sh v0,0xee(s0) */
    goto out;

case8:
    /* 800fcc58: state 8 falls into state 9 after the call. */
    r1 = 2u;                                           /* 800fcc5c addiu a1,zero,2 */
    v0 = RD8(p + 6u);                                  /* 800fcc60 lbu v0,6(s0) */
    WR16(p + 0xCu, 44u);                               /* 800fcc64 addiu v3,zero,44; 800fcc68 sh v3,0xc(s0) */
    v0 += 1u;                                          /* 800fcc6c addiu v0,v0,1 */
    WR8(p + 6u, v0);                                   /* 800fcc74 sb v0,6(s0) (delay slot, before the callee) */
    v0 = m->call4(m->ctx, 0x80025A50u, p, r1, r2, r3); /* 800fcc70 jal 80025a50 */
    keep_leftovers(m, &r1, &r2, &r3);

case9:
    /* 800fcc78: state 9. */
    v0 = (uint32_t)RDS16(p + 0xCu);                    /* 800fcc78 lh v0,0xc(s0) */
    if ((int32_t)v0 != 0) goto cca0;                   /* 800fcc80 bne v0,zero,800fcca0 (delay: v3 = 80) */
    v0 = RD8(p + 6u);                                  /* 800fcc88 lbu v0,6(s0) */
    WR8(p + 1u, 0u);                                   /* 800fcc8c sb zero,1(s0) */
    WR16(p + 0xCu, 80u);                               /* 800fcc90 sh v3,0xc(s0) */
    v0 += 1u;                                          /* 800fcc94 addiu v0,v0,1 */
    goto cc98;
cca0:
    v0 = RD8(p + 1u);                                  /* 800fcca0 lbu v0,1(s0) */
    v1 = RD16(p + 0xCu);                               /* 800fcca4 lhu v1,0xc(s0) */
    v0 = (v0 + 1u) & 1u;                               /* 800fcca8 addiu v0,v0,1; 800fccac andi v0,v0,1 */
    v1 -= 1u;                                          /* 800fccb0 addiu v1,v1,-1 */
    WR8(p + 1u, v0);                                   /* 800fccb4 sb v0,1(s0) */
    WR16(p + 0xCu, v1);                                /* 800fccb8 j 800fcdf4; sh v1,0xc(s0) */
    goto out;

case10:
    /* 800fccc0: state 10. */
    t = (uint32_t)RDS16(p + 0xCu);                     /* 800fccc0 lh v0,0xc(s0) */
    v1 = RD16(p + 0xCu);                               /* 800fccc4 lhu v1,0xc(s0) */
    v0 = v1 - 1u;                                      /* 800fccc8 delay: addiu v0,v1,-1 */
    if ((int32_t)t == 0) goto cd74;                    /* 800fccc8 beq v0(lh value),zero,800fcd74 */
    WR16(p + 0xCu, v0);                                /* 800fccd4 sh v0,0xc(s0) (delay of j) */
    goto out;

case11:
    /* 800fccd8: state 11 falls into state 12 after the call. */
    r1 = 2u;                                           /* 800fccdc addiu a1,zero,2 */
    v0 = RD8(p + 6u);                                  /* 800fcce0 lbu v0,6(s0) */
    WR8(p + 1u, 1u);                                   /* 800fcce4 addiu v1,zero,1; 800fcce8 sb v1,1(s0) */
    v0 += 1u;                                          /* 800fccec addiu v0,v0,1 */
    WR8(p + 6u, v0);                                   /* 800fccf4 sb v0,6(s0) (delay slot, before the callee) */
    v0 = m->call4(m->ctx, 0x80025A50u, p, r1, r2, r3); /* 800fccf0 jal 80025a50 */
    keep_leftovers(m, &r1, &r2, &r3);

case12:
    /* 800fccf8: state 12 (also the fall-through of state 11). */
    sv = RDS8(p + 0xEEu);                              /* 800fccf8 lb v0,0xee(s0) */
    v0 = 255u;                                         /* 800fcd04 delay */
    if (sv <= 0) goto cd78;                            /* 800fcd00 blez v0,800fcd78 */
    v0 = RD16(p + 0xF0u) - 2u;                         /* 800fcd08 lhu v0,0xf0(s0); 800fcd10 addiu v0,v0,-2 */
    WR16(p + 0xEEu, v0);                               /* 800fcd14 sh v0,0xee(s0) */
    sv = RDS8(p + 0xEEu);                              /* 800fcd18 lb v1,0xee(s0) */
    WR16(p + 0xF0u, v0);                               /* 800fcd24 sh v0,0xf0(s0) (delay of bgtz) */
    if (sv > 0) goto out;                              /* 800fcd20 bgtz v1,800fcdf4 */
    WR16(p + 0xF0u, 0u);                               /* 800fcd28 sh zero,0xf0(s0) */
    WR16(p + 0xEEu, 0u);                               /* 800fcd2c sh zero,0xee(s0) (delay of j) */
    goto out;

cd74:
    v0 = 255u;                                         /* 800fcd74 addiu v0,zero,255 */
cd78:
    WR8(p + 6u, v0);                                   /* 800fcd78 j 800fcdf4; sb v0,6(s0) */
    goto out;

case13:
    /* 800fcd34: state 13 falls into state 14 after the call. */
    r1 = 2u;                                           /* 800fcd38 addiu a1,zero,2 */
    v0 = RD8(p + 6u);                                  /* 800fcd3c lbu v0,6(s0) */
    WR8(p + 1u, 1u);                                   /* 800fcd40 addiu v1,zero,1; 800fcd44 sb v1,1(s0) */
    v0 += 1u;                                          /* 800fcd48 addiu v0,v0,1 */
    WR8(p + 6u, v0);                                   /* 800fcd50 sb v0,6(s0) (delay slot, before the callee) */
    v0 = m->call4(m->ctx, 0x80025A50u, p, r1, r2, r3); /* 800fcd4c jal 80025a50 */
    keep_leftovers(m, &r1, &r2, &r3);

case14:
    /* 800fcd54: state 14 (also the fall-through of state 13). */
    v1 = RD16(p + 0xEEu);                              /* 800fcd54 lhu v1,0xee(s0) */
    v0 = 254u;                                         /* 800fcd58 addiu v0,zero,254 */
    if (v1 != v0) goto cd80;                           /* 800fcd5c bne v1,v0,800fcd80 */
    v0 = RD16(p + 0xF0u);                              /* 800fcd64 lhu v0,0xf0(s0) */
    if (v0 != 0u) goto cd80;                           /* 800fcd6c bne v0,zero,800fcd80 */
    goto cd74;

cd80:
    v0 = RD16(p + 0xEEu) + 4u;                         /* 800fcd80 lhu v0,0xee(s0); 800fcd88 addiu v0,v0,4 */
    WR16(p + 0xEEu, v0);                               /* 800fcd8c sh v0,0xee(s0) */
    v0 = RD16(p + 0xF0u);                              /* 800fcd90 lhu v0,0xf0(s0) */
    v1 = RD16(p + 0xEEu);                              /* 800fcd94 lhu v1,0xee(s0) */
    v0 -= 8u;                                          /* 800fcd98 addiu v0,v0,-8 */
    c = (v1 < 255u);                                   /* 800fcd9c sltiu v1,v1,255 */
    WR16(p + 0xF0u, v0);                               /* 800fcda4 sh v0,0xf0(s0) (delay of bne) */
    if (c) goto cdb0;                                  /* 800fcda0 bne v1,zero,800fcdb0 */
    v0 = 254u;                                         /* 800fcda8 addiu v0,zero,254 */
    WR16(p + 0xEEu, v0);                               /* 800fcdac sh v0,0xee(s0) */
cdb0:
    sv = RDS8(p + 0xF0u);                              /* 800fcdb0 lb v0,0xf0(s0) */
    v0 = (uint32_t)sv;
    if (sv >= 0) goto out;                             /* 800fcdb8 bgez v0,800fcdf4 */
    WR16(p + 0xF0u, 0u);                               /* 800fcdc4 sh zero,0xf0(s0) (delay of j) */
    goto out;

dflt:
    /* 800fcdc8: default, clear the bit for P+4 in table 0x191 of S1 and count P+5. */
    v1 = RD8(p + 4u);                                  /* 800fcdc8 lbu v1,4(s0) */
    v0 = 1u << (v1 & 31u);                             /* 800fcdcc addiu v0,zero,1; 800fcdd0 sllv v0,v0,v1 */
    v1 = RD8(q + 0x191u);                              /* 800fcdd4 lbu v1,0x191(s1) */
    v0 = ~v0;                                          /* 800fcdd8 nor v0,zero,v0 */
    v1 &= v0;                                          /* 800fcddc and v1,v1,v0 */
    WR8(q + 0x191u, v1);                               /* 800fcde0 sb v1,0x191(s1) */
    v0 = RD8(p + 5u) + 1u;                             /* 800fcde4 lbu v0,5(s0); 800fcdec addiu v0,v0,1 */
    WR8(p + 5u, v0);                                   /* 800fcdf0 sb v0,5(s0) */
    goto out;

out:
    m->sp_set(m->ctx, sp0 + 40u);
    return v0;
}
