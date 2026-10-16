#include "manual.h"
#include "hex2c_rt.h"

/* VS_SEL 800DFE50..800E00E0 (656 bytes, no calls). $a0 = P. Only $a0 is read before it is written; $a1, $a2, $a3, $t0,
 * $t1, $t2 are set in the function. The MIPS is a chain of branches on byte P+5 and byte P+0xC, with the word at
 * 0x80066730 (loaded into $a1 or read through $t2) choosing bits 0x01000000, 0x00200000, 0x8000, 0x0004.
 * Labels follow the MIPS addresses. Every branch delay slot runs on both paths: where the delay sets a register the
 * branch condition is evaluated first, then the register is set, so the C does the same. */
uint32_t man_VS_SEL_800DFE50(const GuestMem *m, uint32_t a0) {
    uint32_t p = a0;
    uint32_t v0, a1, a2, a3, t0, t1, t2;
    int32_t v1;

    a1 = 0x80060000u;                                     /* lui a1,0x8006 */
    v1 = RDS8(p + 5u);                                    /* lb v1,5(a0) */
    t1 = 18u;                                             /* addiu t1,zero,18 */
    t2 = a1 + 0x6730u;                                    /* addiu t2,a1,26416 (delay slot, both paths) */
    if (v1 != (int32_t)t1)
        goto L_dfed8;
    a1 = RD32(a1 + 0x6730u);                              /* lw a1,0x6730(a1) */
    v0 = a1 & 0x01000000u;                                /* lui v0,0x100; and v0,a1,v0 */
    if (v0 != 0)
        goto L_e00cc;
    v1 = RDS8(p + 0xCu);                                  /* lb v1,0xc(a0) */
    v0 = 15u;                                             /* addiu v0,zero,15 (delay slot of bne, both paths) */
    if (v1 != 0)
        goto L_dfe90;
    goto L_dffa0;                                         /* j dffa0 (delay addiu v0,zero,15) */

L_dfe90:
    if (v1 != 15) {                                       /* bne v1,v0,dfea0 with v0 = 15 */
        v0 = 5u;                                          /* delay addiu v0,zero,5 */
        goto L_dfea0;
    }
    v0 = 5u;                                              /* delay slot, taken path */
    WR8(p + 5u, 0u);                                      /* j e00cc with delay sb zero,5(a0) */
    goto L_e00cc;

L_dfea0:
    if (v1 != 5)                                          /* bne v1,v0,dffa0 with v0 = 5; delay nop */
        goto L_dffa0;
    v0 = a1 & 0x00200000u;                                /* lui v0,0x20; and */
    if (v0 != 0) {
        v0 = 23u;                                         /* bne taken to dffa0; delay addiu v0,zero,23 */
        goto L_dffa0;
    }
    v0 = a1 & 4u;                                         /* andi v0,a1,4 */
    if (v0 != 0) {
        v0 = 24u;                                         /* delay addiu v0,zero,24 */
        goto L_dffa0;
    }
    v0 = a1 & 0x8000u;                                    /* andi v0,a1,0x8000 */
    if (v0 != 0) {
        v0 = 25u;                                         /* delay addiu v0,zero,25 */
        goto L_dffa0;
    }
    v0 = 17u;                                             /* j dffa0, delay addiu v0,zero,17 */
    goto L_dffa0;

L_dfed8:
    t0 = 23u;                                             /* addiu t0,zero,23 */
    a3 = 24u;                                             /* addiu a3,zero,24 (delay of bne at dfedc, both paths) */
    if (v1 != (int32_t)t0)
        goto L_dff70;
    a1 = RD32(a1 + 0x6730u);                              /* lw a1,0x6730(a1) at dfee4 */
    v0 = a1 & 0x00200000u;                                /* lui v0,0x20; and */
    a2 = 11u;                                             /* addiu a2,zero,11 (delay of bne at dfef0, both paths) */
    if (v0 != 0)
        goto L_e00cc;
    v1 = RDS8(p + 0xCu);                                  /* lb v1,0xc(a0) at dfef8 */
    v0 = 14u;                                             /* delay addiu v0,zero,14 (both paths) */
    if (v1 != (int32_t)a2)
        goto L_dff10;
    goto L_dffa0;                                         /* j dffa0, delay addiu v0,zero,14 */

L_dff10:
    if (v1 == 14) {                                       /* beq v1,v0,e00c8 with v0 = 14 */
        v0 = 5u;                                          /* delay addiu v0,zero,5 */
        goto L_e00c8;
    }
    v0 = 5u;                                              /* delay slot, not taken */
    if (v1 == 5) {                                        /* beq v1,v0,dff2c with v0 = 5 */
        v0 = a1 & 4u;                                     /* delay andi v0,a1,4 */
        goto L_dff2c;
    }
    v0 = a1 & 4u;                                         /* delay andi v0,a1,4 */
    if (v1 != (int32_t)t1) {                              /* bne v1,t1,dff3c */
        v0 = 17u;                                         /* delay addiu v0,zero,17 */
        goto L_dff3c;
    }
    v0 = 17u;                                             /* delay, not taken */
    v0 = a1 & 4u;                                         /* dff28: andi v0,a1,4 */

L_dff2c:
    if (v0 != 0) {                                        /* bne v0,zero,dffa0 */
        v0 = 24u;                                         /* delay addiu v0,zero,24 */
        goto L_dffa0;
    }
    v0 = 24u;                                             /* delay, not taken */
    v0 = a1 & 0x8000u;                                    /* j dffd8, delay andi v0,a1,0x8000 */
    goto L_dffd8;

L_dff3c:
    if (v1 == (int32_t)v0) {                              /* beq v1,v0,dff54 with v0 = 17 */
        v0 = 25u;                                         /* delay addiu v0,zero,25 */
        goto L_dff54;
    }
    v0 = 25u;                                             /* delay, not taken */
    if (v1 == 25) {                                       /* beq v1,v0,dff54 with v0 = 25 */
        v0 = 24u;                                         /* delay addiu v0,zero,24 */
        goto L_dff54;
    }
    v0 = 24u;                                             /* delay, not taken */
    if (v1 != (int32_t)v0)                                /* bne v1,v0,e00cc with v0 = 24; delay nop */
        goto L_e00cc;

L_dff54:
    v0 = RD32(t2);                                        /* lw v0,0(t2) */
    v0 = v0 & 0x01000000u;                                /* lui v1,0x100; and v0,v0,v1 */
    if (v0 != 0) {
        v0 = 18u;                                         /* bne taken to dffa0; delay addiu v0,zero,18 */
        goto L_dffa0;
    }
    v0 = 5u;                                              /* dff68: j dffa0 with delay addiu v0,zero,5 (not-taken path) */
    goto L_dffa0;

L_dffa0:
    WR8(p + 5u, v0);                                      /* j e00cc with delay sb v0,5(a0) */
    goto L_e00cc;

L_dff70:
    v0 = 25u;                                             /* delay of bne at dff70 (both paths) */
    if (v1 != (int32_t)a3)                                /* bne v1,a3,e001c */
        goto L_e001c;
    a1 = RD32(a1 + 0x6730u);                              /* lw a1,0x6730(a1) at dff78 */
    v0 = a1 & 4u;                                         /* andi v0,a1,4 */
    a2 = 6u;                                              /* addiu a2,zero,6 (delay of bne at dff84, both paths) */
    if (v0 != 0)
        goto L_e00cc;
    v1 = RDS8(p + 0xCu);                                  /* lb v1,0xc(a0) at dff8c */
    v0 = 19u;                                             /* delay addiu v0,zero,19 (both paths) */
    if (v1 != (int32_t)a2)
        goto L_dffa8;
    goto L_dffa0;                                         /* dff9c, then j dffa0 */

L_dffa8:
    if (v1 == (int32_t)v0)                                /* beq v1,v0,e00c8 with v0 = 19; delay nop */
        goto L_e00c8;
    a2 = 5u;                                              /* addiu a2,zero,5 */
    if (v1 == (int32_t)a2)                                /* beq v1,a2,dffcc; delay nop */
        goto L_dffcc;
    if (v1 == (int32_t)t1)                                /* beq v1,t1,dffcc; delay nop */
        goto L_dffcc;
    if (v1 != (int32_t)t0) {                              /* bne v1,t0,dffe8; delay addiu v0,zero,17 */
        v0 = 17u;
        goto L_dffe8;
    }
    v0 = 17u;                                             /* delay, fall through to dffcc */

L_dffcc:
    v0 = RD32(t2) & 0x8000u;                              /* lw v0,0(t2); andi v0,v0,0x8000 */

L_dffd8:
    if (v0 != 0) {                                        /* bne v0,zero,dffa0; delay addiu v0,zero,25 */
        v0 = 25u;
        goto L_dffa0;
    }
    v0 = 25u;                                             /* delay, not taken */
    v0 = 17u;                                             /* dffe0: j dffa0, delay addiu v0,zero,17 */
    goto L_dffa0;

L_dffe8:
    if (v1 == (int32_t)v0) {                              /* beq v1,v0,dfff8 with v0 = 17 */
        v0 = 25u;                                         /* delay addiu v0,zero,25 */
        goto L_dfff8;
    }
    v0 = 25u;                                             /* delay, not taken */
    if (v1 != (int32_t)v0)                                /* bne v1,v0,e00cc with v0 = 25; delay nop */
        goto L_e00cc;

L_dfff8:
    v0 = a1 & 0x00200000u;                                /* lui v0,0x20; and v0,a1,v0 */
    if (v0 != 0) {                                        /* bne v0,zero,e00ac */
        v0 = 0x01000000u;                                 /* delay lui v0,0x100 */
        goto L_e00ac;
    }
    v0 = 0x01000000u;                                     /* delay, not taken */
    v0 = a1 & v0;                                         /* e0008: and v0,a1,v0 */
    if (v0 != 0)                                          /* bne v0,zero,e00c0; delay nop */
        goto L_e00c0;
    WR8(p + 5u, a2);                                      /* j e00cc with delay sb a2,5(a0) */
    goto L_e00cc;

L_e001c:
    if (v1 != (int32_t)v0)                                /* bne v1,v0,e00d8 with v0 = 25; delay nop */
        goto L_e00d8;
    a1 = RD32(a1 + 0x6730u);                              /* lw a1,0x6730(a1) at e0024 */
    v0 = a1 & 0x8000u;                                    /* andi v0,a1,0x8000 */
    a2 = 27u;                                             /* addiu a2,zero,27 (delay, both paths) */
    if (v0 != 0)
        goto L_e00d8;
    v1 = RDS8(p + 0xCu);                                  /* lb v1,0xc(a0) at e0038 */
    v0 = 10u;                                             /* delay addiu v0,zero,10 (both paths) */
    if (v1 == (int32_t)a2)
        goto L_dffa0;                                     /* e0048: j dffa0 with delay addiu v0,zero,10 */
    if (v1 == (int32_t)v0) {                              /* bne v1,v0,e0064 with v0 = 10 */
        WR8(p + 0x12u, a2);                               /* e0058: sb a2,0x12(a0) */
        WR8(p + 5u, 0u);                                  /* jr ra, delay sb zero,5(a0) */
        return v0;                                        /* v0 = 10 */
    }
    goto L_e0064;

L_e0064:
    a2 = 5u;                                              /* addiu a2,zero,5 */
    if (v1 == (int32_t)a2) {                              /* beq v1,a2,dffa0; delay addiu v0,zero,17 */
        v0 = 17u;
        goto L_dffa0;
    }
    v0 = 17u;                                             /* delay, not taken */
    if (v1 == (int32_t)t1)                                /* beq v1,t1,dffa0; delay nop */
        goto L_dffa0;
    if (v1 == (int32_t)a3)                                /* beq v1,a3,dffa0; delay nop */
        goto L_dffa0;
    if (v1 == (int32_t)t0)                                /* beq v1,t0,dfed0 (j dffa0 with v0 = 17) */
        goto L_dffa0;
    v0 = 17u;                                             /* e0084 delay, both paths */
    if (v1 != (int32_t)v0)                                /* bne v1,v0,e00cc; delay andi v0,a1,4 */
        goto L_e00cc;
    v0 = a1 & 4u;                                         /* andi v0,a1,4 */
    if (v0 == 0) {                                        /* beq v0,zero,e00a0; delay lui v0,0x20 */
        v0 = 0x00200000u;
        goto L_e00a0;
    }
    v0 = 0x00200000u;                                     /* delay, not taken */
    WR8(p + 5u, a3);                                      /* j e00cc; sb a3,5(a0) */
    goto L_e00cc;

L_e00a0:
    v0 = a1 & v0;                                         /* and v0,a1,v0 */
    if (v0 == 0) {                                        /* beq v0,zero,e00b4; delay lui v0,0x100 */
        v0 = 0x01000000u;
        goto L_e00b4;
    }
    v0 = 0x01000000u;                                     /* delay, not taken */
    WR8(p + 5u, t0);                                      /* j e00cc; sb t0,5(a0) (at e00ac/e00b0) */
    goto L_e00cc;

L_e00ac:
    WR8(p + 5u, t0);                                      /* j e00cc with delay sb t0,5(a0) */
    goto L_e00cc;

L_e00b4:
    v0 = a1 & v0;                                         /* and v0,a1,v0 */
    if (v0 == 0)                                          /* beq v0,zero,e00c8; delay nop */
        goto L_e00c8;

L_e00c0:
    WR8(p + 5u, t1);                                      /* j e00cc with delay sb t1,5(a0) */
    goto L_e00cc;

L_e00c8:
    WR8(p + 5u, a2);                                      /* sb a2,5(a0) */

L_e00cc:
    v0 = RD8(p + 5u);                                     /* lbu v0,5(a0) */
    WR8(p + 0x12u, v0);                                   /* sb v0,0x12(a0) */

L_e00d8:
    return v0;                                            /* jr ra */
}
