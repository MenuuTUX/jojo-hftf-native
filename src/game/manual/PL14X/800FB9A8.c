#include "manual.h"
#include "hex2c_rt.h"

#define S8_(x)  ((uint32_t)(int32_t)(int8_t)(x))
#define S16_(x) ((uint32_t)(int32_t)(int16_t)(x))

/* 800FB9A8 (PL14X, slot B, 592 bytes). $a0 = P. $a1..$a3 are never written before a jal, so nargs is 4. Every callee
 * (800fbcc4, 80029714, 80026afc, 80026a04, 800b428c, 80025a50, 800268bc) may change $a0..$a3, so they are reloaded with
 * gpr_get after each call. $v0 at jr ra is the value of the last instruction that set it: a callee's $v0 on the paths that
 * end in a jal (800268bc, 800b428c, 80025a50, 80029714 path through fbb3c), otherwise a computed value. Branch delay slots
 * run on both paths; where a delay slot writes a register the branch tests, the branch reads the value from before it
 * (bne at fb9c4 with addiu v0,zero,1; beq at fba4c with addiu v0,zero,8; bne at fbb18 with addiu v1,zero,-1). */
uint32_t man_PL14X_800FB9A8(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;                                 /* s0 */
    uint32_t s1, v0, v1, t;

#define REFRESH() do {                                       \
        a0 = m->gpr_get(m->ctx, 4u);                         \
        a1 = m->gpr_get(m->ctx, 5u);                         \
        a2 = m->gpr_get(m->ctx, 6u);                         \
        a3 = m->gpr_get(m->ctx, 7u);                         \
    } while (0)

    v1 = RD8(p + 0x06u);                             /* lbu v1,6(s0) */
    s1 = RD32(p + 0x80u);                            /* lw s1,0x80(s0) */
    v0 = 1u;                                         /* addiu v0,zero,1: bne delay slot */
    if (v1 != 0u) goto LBBB0;                        /* bne v1,zero,fbbb0 */

    /* fb9cc: jal 800fbcc4, a0 = P (unchanged), $a1..$a3 as they came in */
    v0 = m->call4(m->ctx, 0x800FBCC4u, p, a1, a2, a3);
    REFRESH();
    a0 = p;                                          /* addu a0,s0,zero: delay slot of bne fb9d4 (both paths) */
    if (v0 != 0u) goto LBBD0;                        /* bne v0,zero,fbbd0 */

    v0 = 0x00FFFF00u;                                /* lui v0,0xff; ori v0,v0,0xff00 */
    v1 = RD32(s1 + 0x04u) & v0;                      /* lw v1,4(s1); and v1,v1,v0 */
    v0 = 0x100u;
    if (v1 != v0) goto LBBD0;                        /* bne v1,v0,fbbd0 */

    v0 = RD8(p + 0x139u);                            /* lbu v0,0x139(s0) */
    if (v0 == 0u) goto LA40;                         /* beq v0,zero,fba40 */

    /* fba08 */
    v0 = RD8(p + 0x06u) + 1u;                        /* lbu v0,6(s0); addiu v0,v0,1 */
    v1 = RD32(p + 0x108u);                           /* lw v1,0x108(s0) */
    WR8(p + 0x06u, v0);                              /* sb v0,6(s0) */
    v1 = RD8(v1 + 0x32u);                            /* lbu v1,0x32(v1) */
    v0 = 8u;
    WR16(p + 0x0Cu, v0);                             /* sh v0,0xc(s0) */
    v0 = 95u;
    WR8(p + 0xF8u, 0u);                              /* sb zero,0xf8(s0) */
    WR16(p + 0xF0u, v0);                             /* sh v0,0xf0(s0) */
    WR16(p + 0xEEu, v0);                             /* sh v0,0xee(s0) */
    v1 = v1 - 1u;                                    /* addiu v1,v1,-1 */
    WR8(p + 0x32u, v1);                              /* j fba7c; sb v1,0x32(s0) in the delay slot */
    goto LBA7C;

LA40:
    v0 = S16_(RD16(p + 0x6Eu));                      /* lh v0,0x6e(s0) */
    t = ((int32_t)v0 < 36);                          /* slti v0,v0,36 */
    v0 = 8u;                                         /* addiu v0,zero,8: beq delay slot */
    if (t == 0u) goto LBAB0;                         /* beq v0(=slti),zero,fbab0 */
    WR16(p + 0x0Cu, v0);                             /* sh v0,0xc(s0) */
    v0 = 35u;
    WR16(p + 0x6Eu, v0);                             /* sh v0,0x6e(s0) */
    v0 = RD8(p + 0x06u);                             /* lbu v0,6(s0) */
    v1 = 95u;
    WR8(p + 0xF8u, 0u);                              /* sb zero,0xf8(s0) */
    WR16(p + 0xF0u, v1);                             /* sh v1,0xf0(s0) */
    WR16(p + 0xEEu, v1);                             /* sh v1,0xee(s0) */
    v0 = v0 + 1u;                                    /* addiu v0,v0,1 */
    WR8(p + 0x06u, v0);                              /* sb v0,6(s0) */

LBA7C:
    v1 = S8_(RD8(s1 + 0x136u));                      /* lb v1,0x136(s1) */
    v0 = v1 << 1;                                    /* sll v0,v1,1 */
    v0 = v0 + v1;                                    /* addu v0,v0,v1 */
    v0 = v0 << 3;                                    /* sll v0,v0,3: 24 * v1 */
    v0 = v0 + 223u;                                  /* addiu v0,v0,223 */
    WR16(p + 0x1Eu, v0);                             /* sh v0,0x1e(s0) */
    /* lui v0,0x8010; lw a1,-0x19cc(v0): a1 = word at 0x800FE634 */
    a1 = RD32(0x800FE634u);
    a0 = p;                                          /* addu a0,s0,zero: delay slot of jal 800268bc */
    v0 = m->call4(m->ctx, 0x800268BCu, a0, a1, a2, a3);
    return v0;                                       /* j fbbe4: returns the callee's $v0 */

LBAB0:
    v0 = RD8(0x80066CA2u);                           /* lui v0,0x8006; lbu v0,0x6ca2(v0) */
    if (v0 != 0u) goto LBBE4;                        /* bne v0,zero,fbbe4 */
    v1 = RD8(p + 0x07u);                             /* lbu v1,7(s0) */
    v0 = 1u;                                         /* addiu v0,zero,1: bne delay slot */
    if (v1 != 0u) goto LBB18;                        /* bne v1,zero,fbb18 */

    /* fbad4 */
    v0 = RD16(p + 0xEEu) + 16u;                      /* lhu v0,0xee(s0); addiu v0,v0,16 */
    WR16(p + 0xEEu, v0);                             /* sh v0,0xee(s0) */
    v0 = RD16(p + 0xF0u);                            /* lhu v0,0xf0(s0) */
    v1 = RD16(p + 0xEEu);                            /* lhu v1,0xee(s0) */
    v0 = v0 + 16u;                                   /* addiu v0,v0,16 */
    v1 = (v1 < 63u);                                 /* sltiu v1,v1,63 */
    WR16(p + 0xF0u, v0);                             /* bne delay slot: sh v0,0xf0(s0) on both paths */
    if (v1 != 0u) goto LBBE4;                        /* bne v1,zero,fbbe4 */
    v0 = 4u;                                         /* addiu v0,zero,4 */
    WR16(p + 0x0Cu, v0);                             /* sh v0,0xc(s0) */
    v0 = RD8(p + 0x07u);                             /* lbu v0,7(s0) */
    v1 = 63u;                                        /* addiu v1,zero,63 */
    WR16(p + 0xF0u, v1);                             /* sh v1,0xf0(s0) */
    WR16(p + 0xEEu, v1);                             /* j fbb74; sh v1,0xee(s0) in the delay slot */
    v0 = v0 + 1u;                                    /* fbb74: addiu v0,v0,1 */
    WR8(p + 0x07u, v0);                              /* j fbbe4; sb v0,7(s0) in the delay slot */
    goto LBBE4;

LBB18:
    t = (v1 != v0);                                  /* bne v1,v0,fbb80: compares before the delay slot */
    v1 = 0xFFFFFFFFu;                                /* addiu v1,zero,-1: delay slot, runs on both paths */
    if (t != 0u) goto LBB80;

    /* fbb20 */
    v0 = RD16(p + 0x0Cu) - 1u;                       /* lhu v0,0xc(s0); addiu v0,v0,-1 */
    WR16(p + 0x0Cu, v0);                             /* sh v0,0xc(s0) */
    v0 = S16_(v0);                                   /* sll/sra 16: signed */
    a0 = s1;                                         /* addu a0,s1,zero: bne delay slot */
    if (v0 != v1) goto LBBE4;                        /* bne v0,v1,fbbe4 */
    a1 = 13u;                                        /* addiu a1,zero,13 */
    a2 = 2u;                                         /* addiu a2,zero,2: jal delay slot */
    v0 = m->call4(m->ctx, 0x80029714u, a0, a1, a2, a3);
    REFRESH();
    v0 = RD8(p + 0x04u) & 1u;                        /* lbu v0,4(s0); andi v0,v0,1 */
    if (v0 == 0u) goto LBB68;                        /* beq v0,zero,fbb68 */
    a0 = p;                                          /* addu a0,s0,zero: jal delay slot */
    m->call4(m->ctx, 0x80026AFCu, a0, a1, a2, a3);   /* jal 80026afc: its $v0 is overwritten below */
    REFRESH();
LBB68:
    v0 = RD8(p + 0x07u);                             /* lbu v0,7(s0) */
    v1 = 2u;                                         /* addiu v1,zero,2 */
    WR16(p + 0x0Cu, v1);                             /* sh v1,0xc(s0) */
    v0 = v0 + 1u;                                    /* fbb74: addiu v0,v0,1 */
    WR8(p + 0x07u, v0);                              /* sb v0,7(s0) */
    goto LBBE4;

LBB80:
    t = S16_(RD16(p + 0x0Cu));                       /* lh v0,0xc(s0): beq at fbb88 tests this */
    v1 = RD16(p + 0x0Cu);                            /* lhu v1,0xc(s0) */
    v0 = v1 - 1u;                                    /* addiu v0,v1,-1: beq delay slot, runs on both paths */
    if (t == 0u) goto LBB98;                         /* beq v0(=t),zero,fbb98 */
    WR16(p + 0x0Cu, v0);                             /* j fbba0; sh v0,0xc(s0) in the delay slot */
    goto LBBA0;
LBB98:
    a0 = p;                                          /* addu a0,s0,zero: jal delay slot */
    m->call4(m->ctx, 0x80026A04u, a0, a1, a2, a3);   /* jal 80026a04 */
    REFRESH();
LBBA0:
    a0 = p;                                          /* addu a0,s0,zero: jal delay slot */
    v0 = m->call4(m->ctx, 0x800B428Cu, a0, a1, a2, a3);
    REFRESH();
    goto LBBE4;

LBBB0:
    if (v1 != v0) goto LBBE4;                        /* bne v1,v0,fbbe4 (v0 = 1 from the entry delay slot) */
    a0 = p;                                          /* nop delay; a0 = P for the jal below */
    m->call4(m->ctx, 0x80026A04u, a0, a1, a2, a3);   /* jal 80026a04, a0 = s0 in the delay slot */
    REFRESH();
    v1 = S8_(RD8(p + 0x4Du));                        /* lb v1,0x4d(s0) */
    v0 = 0xFFFFFFFFu;                                /* addiu v0,zero,-1 */
    if (v1 != v0) goto LBBE4;                        /* bne v1,v0,fbbe4 */
    a0 = p;                                          /* addu a0,s0,zero (delay slot) */

LBBD0:
    v0 = RD8(p + 0x05u) + 1u;                        /* lbu v0,5(a0); addiu v0,v0,1 */
    WR8(p + 0x05u, v0);                              /* sb v0,5(a0): jal delay slot */
    a1 = 2u;                                         /* addiu a1,zero,2 */
    v0 = m->call4(m->ctx, 0x80025A50u, p, a1, a2, a3);
    REFRESH();
    goto LBBE4;

LBBE4:
    return v0;
#undef REFRESH
}
