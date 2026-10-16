#include "manual.h"
#include "hex2c_rt.h"

/* PL01 800E7078..800E760C (1432 bytes, slot A base 800DF000). Per-frame state machine for the object P = a0, with
 * R = RD32(P+0x80), S2 = RD32(R+0x174), S3 = RD32(R+0x17C). Layout follows the MIPS blocks; labels are the MIPS addresses.
 * The 15-way dispatch (taken when RD8(0x80066CA2) == 0) is "jr" through the table at 0x800DF0F4 indexed by RD8(P+6): the
 * table values in extracted/M/PL01.BIN are 800E7134, 800E717C, 800E7294, 800E7310, 800E73C0, 800E740C, 800E75C8 (x2),
 * 800E7458, 800E7478, 800E74C0, 800E74D8, 800E74F8, 800E7534, 800E7554; a switch on the index is the same jump.
 * The return value is the $v0 at 800e75f4 on each path (set as the MIPS sets it, including the callees' return values).
 * $a2 and $a3 are not written by this function and pass through. $a1 is the value the MIPS last set; across calls that
 * is not modelled (GUESS: callee clobbers of $a1 are not seen).
 * The generated C was a Hex-Rays-style flattening with loaded words read again (see the review notes in the summary). */
uint32_t man_PL01_800E7078(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0, r, s2, s3, v0 = 0u, v1 = 0u, v3 = 0u, a1v = a1, ra0, t;
    uint32_t f;

    f = RD8(0x80066CA2u);                           /* lui v0,0x8006; lbu v0,0x6ca2(v0) */
    r = RD32(p + 0x80u);                            /* lw s1,0x80(s0) */
    s2 = RD32(r + 0x174u);                          /* lw s2,0x174(s1) */
    s3 = RD32(r + 0x17Cu);                          /* lw s3,0x17c(s1) */
    if (f == 0u) {
        goto L7100;                                 /* beq v0,zero,800e7100 */
    }

    /* 800e70b0: frame flag set */
    v0 = RD32(p + 0x108u);                          /* lw v0,0x108(s0) */
    if (v0 == 0u) {                                 /* beq v0,zero,800e75f4; delay addiu v0,zero,1 runs first */
        return 1u;
    }
    v0 = 1u;                                        /* addiu v0,zero,1 (delay of the beq above, also on this path) */
    v3 = RD8(p + 0x04u);                            /* lbu v1,0x4(s0): the shift count */
    v0 = v0 << (v3 & 31u);                          /* sllv v0,v0,v3 */
    v1 = RD8(r + 0x191u);                           /* lbu v1,0x191(s1) */
    v0 = ~v0;                                       /* nor v0,zero,v0 */
    v1 = v1 & v0;                                   /* and v1,v1,v0 */
    WR8(r + 0x191u, v1);                            /* sb v1,0x191(s1) */
    v0 = RD8(p + 0x05u);                            /* lbu v0,0x5(s0) */
    v3 = RD8(p + 0x17u);                            /* lbu v1,0x17(s0) */
    v0 = v0 + 1u;                                   /* addiu v0,v0,1 */
    if (v3 == 0u) {                                 /* beq v1,zero,800e75f4; delay sb v0,0x5(s0) runs first */
        WR8(p + 0x05u, v0);
        return v0;
    }
    WR8(p + 0x05u, v0);                             /* sb v0,0x5(s0) (delay of the beq, before the jal) */
    v0 = m->call4(m->ctx, 0x80025A50u, p, 2u, a2, a3);     /* jal 80025a50; delay addiu a1,zero,2 */
    return v0;                                      /* j 800e75f4 */

L7100:
    /* 800e7100: frame flag clear */
    v0 = RD16(r + 0x1Eu);                           /* lhu v0,0x1e(s1) */
    WR16(p + 0x1Eu, v0);                            /* sh v0,0x1e(s0) */
    v1 = RD8(p + 0x06u);                            /* lbu v1,0x6(s0) */
    if (v1 >= 15u) {                                /* sltiu v0,v1,15; beq v0,zero,800e75c8 (delay lui v0,0x800e) */
        goto L75C8;
    }
    switch (v1) {                                   /* lui v0,0x800e; addiu; sll v1,v1,2; addu; lw v0,0(v1); jr v0 */
    case 0u:  goto L7134;
    case 1u:  goto L717C;
    case 2u:  goto L7294;
    case 3u:  goto L7310;
    case 4u:  goto L73C0;
    case 5u:  goto L740C;
    case 6u:
    case 7u:  goto L75C8;
    case 8u:  goto L7458;
    case 9u:  goto L7478;
    case 10u: goto L74C0;
    case 11u: goto L74D8;
    case 12u: goto L74F8;
    case 13u: goto L7534;
    case 14u: goto L7554;
    default:  goto L75C8;
    }

L7134:
    /* 800e7134: case 0 */
    v0 = RD8(p + 0x06u) + 1u;                       /* lbu v0,6(s0); addiu v0,v0,1 */
    WR8(p + 0x06u, v0);                             /* sb v0,6(s0) */
    v0 = RD8(s2 + 0x1FDu);                          /* lbu v0,0x1fd(s2) */
    if (v0 == 0u) {                                 /* beq v0,zero,800e7158; delay addiu v3,zero,1600 */
        v3 = 1600u;
    } else {
        v3 = 600u;                                  /* 800e7154: addiu v3,zero,600 (not-taken path) */
    }

    /* 800e7158 (fall-through from 800e7134) */
    a1v = RD8(p + 0x04u);                           /* lbu a1,0x4(s0) */
    WR16(p + 0x0Cu, v3);                            /* sh v3,0xc(s0) (after addu a0,s0) */
    a1v = (a1v + 41u) & 0xFFu;                      /* addiu a1,a1,41; delay andi a1,a1,0xff */
    (void)m->call4(m->ctx, 0x800BB2A4u, p, a1v, a2, a3);   /* jal 800bb2a4 */
    a1v = 2u;                                       /* addiu a1,zero,2 (delay of the jal below) */
    (void)m->call4(m->ctx, 0x800258F0u, p, a1v, a2, a3);   /* jal 800258f0 */

L717C:
    /* 800e717c: case 1 (also entered from the table) */
    v1 = RD32(s2 + 0x04u) & 0x00FFFF00u;            /* lui v0,0xff; lw v1,4(s2); ori v0,v0,0xff00; and v1,v1,v0 */
    if (v1 != 256u) {                               /* bne v1,v0(=256),800e7394; delay addiu v0,zero,11 */
        v0 = 11u;
        goto L7394;
    }
    v0 = 11u;                                       /* delay slot, also on the fall-through path */
    /* 800e7198 */
    ra0 = (uint32_t)(int32_t)RDS16(0x80066CEAu);    /* lui v0,0x8006; lh a0,0x6cea(v0) */
    v3 = (uint32_t)(int32_t)RDS16(p + 0x6Au);       /* lh v3,0x6a(s0) (MIPS v1 here is v3) */
    v0 = ra0 + 480u;                                /* addiu v0,a0,480 */
    if ((int32_t)v0 < (int32_t)v3) {                /* slt v0,v0,v3; bne v0,zero,800e7394; delay addiu v0,zero,11 */
        v0 = 11u;
        goto L7394;
    }
    v0 = 11u;
    v0 = ra0 - 96u;                                 /* addiu v0,a0,-96 */
    if ((int32_t)v3 < (int32_t)v0) {                /* slt v0,v3,v0; bne v0,zero,800e7394; delay addiu v0,zero,11 */
        v0 = 11u;
        goto L7394;
    }
    /* 800e71c4 */
    t = RD32(p + 0x108u);                           /* lw v0,0x108(s0) */
    v0 = 13u;                                       /* bne v0,zero,800e73ac; delay addiu v0,zero,13 */
    if (t != 0u) {
        goto L73AC;
    }
    /* 800e71d4 */
    if ((int32_t)RDS16(p + 0x0Cu) == 0) {           /* lh v0,0xc(s0); beq v0,zero,800e7248 */
        goto L7248;
    }
    /* 800e71e4 */
    v0 = RD8(s2 + 0x1FDu);                          /* lbu v0,0x1fd(s2) */
    if (v0 != 0u) {                                 /* bne v0,zero,800e7224; delay lui v0,0x800f */
        goto L7224;
    }
    v0 = 0x800F0000u;                               /* lui v0,0x800f (delay slot) */
    v3 = RD8(p + 0x04u);                            /* lbu v3,0x4(s0) */
    v0 = 0x800EA110u;                               /* addiu v0,v0,-24304 */
    v3 = v3 << 1;                                   /* sll v3,v3,1 */
    v3 = v3 + v0;                                   /* addu v3,v3,v0 */
    v0 = RD16(r + 0x1CCu);                          /* lhu v0,0x1cc(s1) */
    v3 = RD16(v3);                                  /* lhu v3,0(v3) */
    v0 = v0 & v3;                                   /* and v0,v0,v3 */
    if (v0 == 0u) {                                 /* beq v0,zero,800e7248 */
        goto L7248;
    }
    goto L7280;                                     /* j 800e7280 */

L7224:
    /* 800e7224 */
    v0 = (uint32_t)(int32_t)RDS16(s3 + 0x6Au);      /* lh v0,0x6a(s3) */
    v0 = v0 - v3;                                   /* subu v0,v0,v3 (v3 = P+0x6A from above) */
    if ((int32_t)v0 < 0) {                          /* bgez v0,800e723c not taken */
        v0 = 0u - v0;                               /* subu v0,zero,v0 */
    }
    if (!((int32_t)v0 < 384)) {                     /* slti v0,v0,384; beq v0,zero,800e7280 */
        goto L7280;
    }
    goto L7248;

L7248:
    /* 800e7248 */
    v0 = RD8(p + 0x06u);                            /* lbu v0,0x6(s0) */
    v3 = 1u;                                        /* addiu v3,zero,1 */
    WR8(p + 0x01u, v3);                             /* sb v3,0x1(s0) */
    WR16(p + 0xF0u, 0u);                            /* sh zero,0xf0(s0) */
    WR16(p + 0xEEu, 0u);                            /* sh zero,0xee(s0) */
    v0 = v0 + 1u;                                   /* addiu v0,v0,1 */
    WR8(p + 0x06u, v0);                             /* sb v0,0x6(s0) */
    a1v = RD32(0x800EDC7Cu);                        /* lui v0,0x800f; lw a1,-0x2384(v0) */
    (void)m->call4(m->ctx, 0x800268BCu, p, a1v, a2, a3);   /* jal 800268bc */
    a1v = 299u;                                     /* addiu a1,zero,299 (delay of the jal below) */
    (void)m->call4(m->ctx, 0x80024104u, p, a1v, a2, a3);   /* jal 80024104 */

L7280:
    /* 800e7280 */
    v0 = RD16(p + 0x0Cu) - 1u;                      /* lhu v0,0xc(s0); addiu v0,v0,-1 */
    WR16(p + 0x0Cu, v0);                            /* j 800e75f4; delay sh v0,0xc(s0) */
    return v0;

L73AC:
    /* 800e73ac */
    WR8(p + 0x06u, v0);                             /* sb v0,0x6(s0) (v0 = 13 here) */
    return v0;

L7394:
    /* 800e7394 */
    WR8(p + 0x06u, v0);                             /* sb v0,0x6(s0) (v0 = 11 or 13) */
    return v0;

L7294:
    /* 800e7294: case 2 */
    v1 = RD32(s2 + 0x04u) & 0x00FFFF00u;            /* lui v0,0xff; lw v3,4(s2); ori; and */
    if (v1 != 256u) {                               /* bne v3,v0,800e7394; delay addiu v0,zero,11 */
        v0 = 11u;
        goto L7394;
    }
    v0 = 11u;
    v0 = RD16(p + 0xF0u) + 10u;                     /* lhu v0,0xf0(s0); addiu v0,v0,10 */
    WR16(p + 0xF0u, v0);                            /* sh v0,0xf0(s0) */
    WR16(p + 0xEEu, v0);                            /* sh v0,0xee(s0) */
    v0 = v0 & 0xFFFFu;                              /* andi v0,v0,0xffff */
    v3 = 126u;                                      /* delay addiu v3,zero,126 (both paths) */
    if (v0 < 126u) {                                /* sltiu v0,v0,126; bne v0,zero,800e7300 */
        goto L7300;
    }
    v0 = RD8(p + 0x06u);                            /* lbu v0,0x6(s0) */
    WR16(p + 0xF0u, v3);                            /* sh v3,0xf0(s0) */
    WR16(p + 0xEEu, v3);                            /* sh v3,0xee(s0) */
    v0 = v0 + 1u;                                   /* addiu v0,v0,1 */
    WR8(p + 0x06u, v0);                             /* sb v0,0x6(s0) */
    a1v = RD32(0x800EDC90u);                        /* lui v0,0x800f; lw a1,-0x2370(v0) */
    (void)m->call4(m->ctx, 0x800268BCu, p, a1v, a2, a3);   /* jal 800268bc */
    v0 = 30u;                                       /* addiu v0,zero,30 */
    WR16(p + 0x0Cu, v0);                            /* sh v0,0xc(s0) */

L7300:
    /* 800e7300 */
    v0 = m->call4(m->ctx, 0x80026A04u, p, a1v, a2, a3);   /* jal 80026a04; j 800e75f4 with its $v0 */
    return v0;

L7310:
    /* 800e7310: case 3 */
    v0 = RD8(p + 0x139u);                           /* lbu v0,0x139(s0) */
    if (v0 == 0u) {                                 /* beq v0,zero,800e733c */
        goto L733C;
    }
    v0 = RD8(p + 0x06u);                            /* lbu v0,6(s0) */
    a1v = 300u;                                     /* addiu a1,zero,300 */
    v0 = v0 + 1u;                                   /* addiu v0,v0,1 */
    WR8(p + 0x06u, v0);                             /* sb v0,6(s0) (delay of the jal) */
    v0 = m->call4(m->ctx, 0x80024104u, p, a1v, a2, a3);   /* jal 80024104 */
    WR16(p + 0x100u, 0u);                           /* j 800e75f4; delay sh zero,0x100(s0) */
    return v0;

L733C:
    /* 800e733c */
    v0 = (uint32_t)(int32_t)RDS16(p + 0x0Cu);       /* lh v0,0xc(s0) */
    a1v = RD16(p + 0x0Cu);                          /* lhu a1,0xc(s0) */
    if (v0 == 0u) {                                 /* beq v0,zero,800e7390; delay lui v0,0xff */
        goto L7390;
    }
    v0 = 0x00FF0000u;                               /* lui v0,0xff (delay slot) */
    v1 = RD32(s2 + 0x04u) & 0x00FFFF00u;            /* lw v3,4(s2); ori v0,v0,0xff00; and */
    if (v1 != 256u) {                               /* bne v3,v0,800e7394; delay addiu v0,zero,11 */
        v0 = 11u;
        goto L7394;
    }
    v0 = 11u;
    ra0 = (uint32_t)(int32_t)RDS16(0x80066CEAu);    /* lui v0,0x8006; lh a0,0x6cea(v0) */
    v3 = (uint32_t)(int32_t)RDS16(p + 0x6Au);       /* lh v3,0x6a(s0) */
    v0 = ra0 + 480u;                                /* addiu v0,a0,480 */
    if ((int32_t)v0 < (int32_t)v3) {                /* slt v0,v0,v3; bne v0,zero,800e7394; delay addiu v0,zero,11 */
        v0 = 11u;
        goto L7394;
    }
    v0 = 11u;
    v0 = ra0 - 96u;                                 /* addiu v0,a0,-96 */
    if (!((int32_t)v3 < (int32_t)v0)) {             /* slt v0,v3,v0; beq v0,zero,800e739c */
        goto L739C;
    }

L7390:
    /* 800e7390 */
    v0 = 11u;
    goto L7394;

L739C:
    /* 800e739c */
    t = RD32(p + 0x108u);                           /* lw v0,0x108(s0) */
    v0 = 13u;                                       /* delay addiu v0,zero,13 */
    if (t == 0u) {                                  /* beq v0,zero,800e73b4 (t compared before the delay slot) */
        goto L73B4;
    }
    goto L73AC;                                     /* 800e73ac */

L73B4:
    /* 800e73b4 */
    v0 = a1v - 1u;                                  /* addiu v0,a1,-1 */
    WR16(p + 0x0Cu, v0);                            /* j 800e75f4; delay sh v0,0xc(s0) */
    return v0;

L73C0:
    /* 800e73c0: case 4 */
    v0 = RD16(p + 0xEEu);                           /* lhu v0,0xee(s0) */
    if (v0 >= 254u) {                               /* sltiu v0,v0,254; beq v0,zero,800e7400 */
        goto L7400;
    }
    v0 = RD16(p + 0xF0u) + 16u;                     /* lhu v0,0xf0(s0); addiu v0,v0,16 */
    WR16(p + 0xF0u, v0);                            /* sh v0,0xf0(s0) */
    WR16(p + 0xEEu, v0);                            /* sh v0,0xee(s0) */
    v0 = v0 & 0xFFFFu;                              /* andi v0,v0,0xffff */
    t = (v0 < 255u);                                /* sltiu v0,v0,255 */
    v0 = 254u;                                      /* addiu v0,zero,254 (delay of the bne) */
    if (t) {                                        /* bne v0,zero,800e75f4 */
        return v0;
    }
    WR16(p + 0xF0u, v0);                            /* j 800e7448; delay sh v0,0xf0(s0) */
    WR16(p + 0xEEu, v0);                            /* 800e7448: sh v0,0xee(s0); j 800e75f4 */
    return v0;

L7400:
    /* 800e7400 */
    v0 = RD8(p + 0x06u) + 1u;                       /* lbu v0,6(s0); j 800e7498; delay addiu v0,v0,1 */
    WR8(p + 0x06u, v0);                             /* 800e7498: j 800e75f4; delay sb v0,6(s0) */
    return v0;

L740C:
    /* 800e740c: case 5 */
    v0 = RD16(p + 0xEEu);                           /* lhu v0,0xee(s0) */
    if (v0 < 223u) {                                /* sltiu v0,v0,223; bne v0,zero,800e7450; delay addiu v0,zero,8 */
        goto L7450;
    }
    v0 = RD16(p + 0xF0u) - 8u;                      /* lhu v0,0xf0(s0); addiu v0,v0,-8 */
    WR16(p + 0xF0u, v0);                            /* sh v0,0xf0(s0) */
    WR16(p + 0xEEu, v0);                            /* sh v0,0xee(s0) */
    v0 = v0 & 0xFFFFu;                              /* andi v0,v0,0xffff */
    if (v0 >= 222u) {                               /* sltiu v0,v0,222; beq v0,zero,800e75f4; delay addiu v0,zero,222 */
        return 222u;
    }
    v0 = 222u;                                      /* addiu v0,zero,222 (delay slot, not-taken path) */
    WR16(p + 0xF0u, v0);                            /* sh v0,0xf0(s0) */
    WR16(p + 0xEEu, v0);                            /* sh v0,0xee(s0) */
    return v0;

L7450:
    /* 800e7450 */
    WR8(p + 0x06u, 8u);                             /* j 800e75f4; delay sb v0,6(s0) with v0 = 8 */
    return 8u;

L7458:
    /* 800e7458: case 8 */
    a1v = 2u;                                       /* addiu a1,zero,2 (after addu a0,s0,zero) */
    v0 = RD8(p + 0x06u);                            /* lbu v0,6(s0) */
    v1 = 44u;                                       /* addiu v1,zero,44 */
    WR16(p + 0x0Cu, v1);                            /* sh v1,0xc(s0) */
    v0 = v0 + 1u;                                   /* addiu v0,v0,1 */
    WR8(p + 0x06u, v0);                             /* sb v0,6(s0) (delay of the jal) */
    (void)m->call4(m->ctx, 0x80025A50u, p, a1v, a2, a3);   /* jal 80025a50 */

L7478:
    /* 800e7478: case 9 (also the fall-through of case 8) */
    v0 = (uint32_t)(int32_t)RDS16(p + 0x0Cu);       /* lh v0,0xc(s0) */
    v3 = 80u;                                       /* addiu v3,zero,80 (delay slot) */
    if (v0 != 0u) {                                 /* bne v0,zero,800e74a0 */
        goto L74A0;
    }
    v0 = RD8(p + 0x06u);                            /* 800e7488: lbu v0,6(s0) */
    WR8(p + 0x01u, 0u);                             /* sb zero,0x1(s0) */
    WR16(p + 0x0Cu, v3);                            /* sh v3,0xc(s0) */
    v0 = v0 + 1u;                                   /* addiu v0,v0,1 */
    WR8(p + 0x06u, v0);                             /* j 800e75f4; delay sb v0,6(s0) */
    return v0;

L74A0:
    /* 800e74a0 */
    v0 = RD8(p + 0x01u);                            /* lbu v0,1(s0) */
    v1 = RD16(p + 0x0Cu);                           /* lhu v1,0xc(s0) */
    v0 = v0 + 1u;                                   /* addiu v0,v0,1 */
    v0 = v0 & 1u;                                   /* andi v0,v0,0x1 */
    v1 = v1 - 1u;                                   /* addiu v1,v1,-1 */
    WR8(p + 0x01u, v0);                             /* sb v0,1(s0) */
    WR16(p + 0x0Cu, v1);                            /* j 800e75f4; delay sh v1,0xc(s0) */
    return v0;

L74C0:
    /* 800e74c0: case 10 */
    t = (uint32_t)(int32_t)RDS16(p + 0x0Cu);        /* lh v0,0xc(s0) */
    v1 = RD16(p + 0x0Cu);                           /* lhu v1,0xc(s0) */
    v0 = v1 - 1u;                                   /* addiu v0,v1,-1 (delay slot of the beq) */
    if (t == 0u) {                                  /* beq v0,zero,800e7574 (tested on the lh value) */
        goto L7574;
    }
    WR16(p + 0x0Cu, v0);                            /* j 800e75f4; delay sh v0,0xc(s0) */
    return v0;

L74D8:
    /* 800e74d8: case 11 */
    a1v = 2u;                                       /* addiu a1,zero,2 */
    v0 = RD8(p + 0x06u);                            /* lbu v0,6(s0) */
    v3 = 1u;                                        /* addiu v3,zero,1 */
    WR8(p + 0x01u, v3);                             /* sb v3,1(s0) */
    v0 = v0 + 1u;                                   /* addiu v0,v0,1 */
    WR8(p + 0x06u, v0);                             /* sb v0,6(s0) (delay of the jal) */
    (void)m->call4(m->ctx, 0x80025A50u, p, a1v, a2, a3);   /* jal 80025a50 */

L74F8:
    /* 800e74f8: case 12 (also the fall-through of case 11) */
    t = (uint32_t)(int32_t)RDS8(p + 0xEEu);         /* lb v0,0xee(s0) */
    v0 = 255u;                                      /* addiu v0,zero,255 (delay slot of the blez) */
    if ((int32_t)t <= 0) {                          /* blez v0,800e7578 */
        goto L7578;
    }
    v0 = RD16(p + 0xF0u) - 2u;                      /* lhu v0,0xf0(s0); addiu v0,v0,-2 */
    WR16(p + 0xEEu, v0);                            /* sh v0,0xee(s0) */
    t = (uint32_t)(int32_t)RDS8(p + 0xEEu);        /* lb v3,0xee(s0) */
    if ((int32_t)t > 0) {                           /* bgtz v3,800e75f4; delay sh v0,0xf0(s0) */
        WR16(p + 0xF0u, v0);
        return v0;
    }
    WR16(p + 0xF0u, 0u);                            /* sh zero,0xf0(s0) */
    WR16(p + 0xEEu, 0u);                            /* j 800e75f4; delay sh zero,0xee(s0) */
    return v0;

L7534:
    /* 800e7534: case 13 */
    a1v = 2u;                                       /* addiu a1,zero,2 */
    v0 = RD8(p + 0x06u);                            /* lbu v0,6(s0) */
    v3 = 1u;                                        /* addiu v3,zero,1 */
    WR8(p + 0x01u, v3);                             /* sb v3,1(s0) */
    v0 = v0 + 1u;                                   /* addiu v0,v0,1 */
    WR8(p + 0x06u, v0);                             /* sb v0,6(s0) (delay of the jal) */
    (void)m->call4(m->ctx, 0x80025A50u, p, a1v, a2, a3);   /* jal 80025a50 */

L7554:
    /* 800e7554: case 14 (also the fall-through of case 13) */
    v3 = RD16(p + 0xEEu);                           /* lhu v3,0xee(s0) */
    v0 = 254u;                                      /* addiu v0,zero,254 */
    if (v3 != 254u) {                               /* bne v3,v0,800e7580 */
        goto L7580;
    }
    v0 = RD16(p + 0xF0u);                           /* lhu v0,0xf0(s0) */
    if (v0 != 0u) {                                 /* bne v0,zero,800e7580 */
        goto L7580;
    }
    goto L7574;

L7580:
    /* 800e7580 */
    v0 = RD16(p + 0xEEu) + 4u;                      /* lhu v0,0xee(s0); addiu v0,v0,4 */
    WR16(p + 0xEEu, v0);                            /* sh v0,0xee(s0) */
    v0 = RD16(p + 0xF0u);                           /* lhu v0,0xf0(s0) */
    v3 = RD16(p + 0xEEu);                           /* lhu v3,0xee(s0) */
    v0 = v0 - 8u;                                   /* addiu v0,v0,-8 */
    t = (v3 < 255u);                                /* sltiu v3,v3,255 */
    WR16(p + 0xF0u, v0);                            /* sh v0,0xf0(s0) (delay of the bne) */
    if (!t) {                                       /* bne v3,zero,800e75b0 not taken */
        v0 = 254u;                                  /* addiu v0,zero,254 */
        WR16(p + 0xEEu, v0);                        /* sh v0,0xee(s0) */
    }
    /* 800e75b0 */
    t = (uint32_t)(int32_t)RDS8(p + 0xF0u);        /* lb v0,0xf0(s0) */
    v0 = t;
    if ((int32_t)t >= 0) {                          /* bgez v0,800e75f4 (delay nop) */
        return v0;
    }
    WR16(p + 0xF0u, 0u);                            /* j 800e75f4; delay sh zero,0xf0(s0) */
    return v0;

L75C8:
    /* 800e75c8: default, also cases 6 and 7 */
    v3 = RD8(p + 0x04u);                            /* lbu v3,0x4(s0) */
    v0 = 1u;                                        /* addiu v0,zero,1 */
    v0 = v0 << (v3 & 31u);                          /* sllv v0,v0,v3 */
    v1 = RD8(r + 0x191u);                           /* lbu v1,0x191(s1) */
    v0 = ~v0;                                       /* nor v0,zero,v0 */
    v1 = v1 & v0;                                   /* and v1,v1,v0 */
    WR8(r + 0x191u, v1);                            /* sb v1,0x191(s1) */
    v0 = RD8(p + 0x05u) + 1u;                       /* lbu v0,5(s0); addiu v0,v0,1 */
    WR8(p + 0x05u, v0);                             /* sb v0,5(s0) */
    return v0;

L7574:
    /* 800e7574 */
    v0 = 255u;                                      /* addiu v0,zero,255 */
L7578:
    /* 800e7578: j 800e75f4; delay sb v0,6(s0) */
    WR8(p + 0x06u, v0);
    return v0;
}
