#include "manual.h"
#include "hex2c_rt.h"

/* 800A2D74 (GAMESS), 8 bytes: lui v0,0x8006; lbu v1,0x6c6b(v0). It falls through into 800A2D7C (444 bytes, frame 0x28),
 * so both are written here as one unit. The stub sets $v1 (no $v1 input); $v0 is written before it is read (no $v0 input).
 * $a0 is read (addu s0,a0). $a1, $a2, $a3 are passed on unchanged to 800A304C on some paths and to 8002438C on others
 * (on the paths that do not reload them, see 800a2d88..2e04 and 800a2e2c), so nargs is 4.
 * The frame keeps the local 8-byte copy at sp+0x10..sp+0x17 (+ halfword at sp+0x18), read back as the halfword at sp+0x16.
 * Returns the $v0 at jr ra. */
uint32_t man_800A2D74(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp, s0, v0, v1, t, t0;

    sp = m->sp_get(m->ctx) - 0x28u;             /* addiu sp,sp,-40 */
    m->sp_set(m->ctx, sp);
    s0 = a0;                                    /* addu s0,a0,zero */

    v1 = RD8(0x80066C6Bu);                      /* stub: lbu v1,0x6c6b(v0), v0 = 0x80060000 */
    v0 = v1 & 1u;                               /* andi v0,v1,0x1 */
    if (v0 == 0) goto L_2DCC;                   /* beq v0,zero,800a2dcc */
    /* lui v0,0x8007; addiu t1,v0,-12616 -> t1 = 0x8006CEB8 (lwl/lwr pairs are whole aligned words) */
    a2 = RD32(0x8006CEB8u);                     /* lwl/lwr a2,(t1) */
    a3 = RD32(0x8006CEBCu);                     /* lwl/lwr a3,4(t1) */
    t0 = (uint32_t)RDS16(0x8006CEC0u);          /* lh t0,8(t1) */
    WR32(sp + 0x10u, a2);                       /* swl/swr a2 -> sp+0x10 */
    WR32(sp + 0x14u, a3);                       /* swl/swr a3 -> sp+0x14 */
    WR16(sp + 0x18u, t0);                       /* sh t0,0x18(sp) */
    goto L_2E04;                                /* j 800a2e04 */

L_2DCC:
    /* 800a2dcc */
    v0 = v1 & 2u;                               /* andi v0,v1,0x2 */
    if (v0 == 0) goto L_2E04;                   /* beq v0,zero,800a2e04 */
    /* lui v0,0x8007 (delay slot); addiu t1,v0,-12600 -> t1 = 0x8006CEC8 */
    a2 = RD32(0x8006CEC8u);
    a3 = RD32(0x8006CECCu);
    t0 = (uint32_t)RDS16(0x8006CED0u);
    WR32(sp + 0x10u, a2);
    WR32(sp + 0x14u, a3);
    WR16(sp + 0x18u, t0);

L_2E04:
    /* 800a2e04 */
    v1 = RD16(sp + 0x16u);                      /* lhu v1,0x16(sp) */
    v0 = v1 & 0x800u;                           /* andi v0,v1,0x800 */
    a0 = 138u;                                  /* addiu a0,zero,138 (delay slot of beq at 2e10) */
    if (v0 == 0) goto L_2E2C;                   /* beq v0,zero,800a2e2c */
    v0 = RD16(s0 + 0x4u);                       /* lhu v0,0x4(s0) */
    WR16(s0 + 0x6u, 0u);                        /* sh zero,0x6(s0) */
    v0 = v0 + 1u;                               /* addiu v0,v0,1 */
    WR16(s0 + 0x4u, v0);                        /* 800a2e28 (delay slot of j 800a2f18) */
    goto L_2F18;                                /* j 800a2f18 */

L_2E2C:
    /* 800a2e2c */
    v0 = 0x800A0000u;                           /* lui v0,0x800a */
    a1 = RD32(0x800A361Cu);                     /* lw a1,0x361c(v0) */
    a0 = (uint32_t)RDS8(a1);                    /* lb a0,0(a1) */
    v0 = 7u;                                    /* addiu v0,zero,7 (delay slot of bne at 2e40) */
    if (a0 != 3u) goto L_2E70;                  /* bne a0,v0(=3),800a2e70 */

    /* a0 == 3 */
    v0 = v1 & 0x40u;                            /* andi v0,v1,0x40 */
    t = v0;
    v0 = v1 & 0x10u;                            /* 800a2e50 (delay slot of beq at 2e4c) */
    if (t == 0) goto L_2EF8;                    /* beq v0,zero,800a2ef8 */
    v1 = RD16(s0 + 0x8u);                       /* lhu v1,0x8(s0) */
    v0 = (v1 < 2u) ? 1u : 0u;                   /* sltiu v0,v1,2 */
    t = v0;
    v0 = v1 + 1u;                               /* 800a2e64 (delay slot of beq at 2e60) */
    if (t == 0) goto L_2F20;                    /* beq v0,zero,800a2f20 (tests sltiu result) */
    WR16(s0 + 0x8u, v0);                        /* 800a2e6c (delay slot of j 800a2f14) */
    goto L_2F14;

L_2E70:
    /* 800a2e70: v0 = 7 */
    if (a0 != 7u) goto L_2F20;                  /* bne a0,v0(=7),800a2f20 */
    v0 = 1u;                                    /* addiu v0,zero,1 (delay slot) */
    a0 = (uint32_t)RD8(a1 + 0x5u);              /* lbu a0,0x5(a1) */
    v0 = 2u;                                    /* addiu v0,zero,2 (delay slot of bne at 2e80, which tests v0 = 1) */
    if (a0 != 1u) goto L_2ECC;                  /* bne a0,v0(=1),800a2ecc */

    /* a0 == 1 */
    v0 = v1 & 0x40u;                            /* andi v0,v1,0x40 */
    t = v0;
    v0 = v1 & 0x10u;                            /* 800a2e90 (delay slot of beq at 2e8c) */
    if (t == 0) goto L_2EAC;                    /* beq v0,zero,800a2eac */
    v0 = RD16(s0 + 0x8u);                       /* lhu v0,0x8(s0) */
    t = v0;
    v0 = v0 + 1u;                               /* 800a2ea0 (delay slot of bne at 2e9c) */
    if (t != 0) goto L_2F20;                    /* bne v0,zero,800a2f20 (tests the loaded value) */
    WR16(s0 + 0x8u, v0);                        /* 800a2ea8 (delay slot of j 800a2f14) */
    goto L_2F14;

L_2EAC:
    /* 800a2eac: v0 = v1 & 0x10 */
    if (v0 == 0) goto L_2F20;                   /* beq v0,zero,800a2f20 */
    v0 = RD16(s0 + 0x8u);                       /* lhu v0,0x8(s0) */
    t = v0;
    v0 = v0 - 1u;                               /* 800a2ec0 (delay slot of bne at 2ebc) */
    if (t != a0) goto L_2F20;                   /* bne v0,a0,800a2f20 (a0 == 1 here) */
    WR16(s0 + 0x8u, v0);                        /* 800a2ec8 (delay slot of j 800a2f14) */
    goto L_2F14;

L_2ECC:
    /* 800a2ecc: v0 = 2 */
    if (a0 != 2u) goto L_2F20;                  /* bne a0,v0(=2),800a2f20 */
    v0 = v1 & 0x40u;                            /* andi v0,v1,0x40 (delay slot) */
    t = v0;
    v0 = v1 & 0x10u;                            /* 800a2ed8 (delay slot of beq at 2ed4) */
    if (t == 0) goto L_2EF8;                    /* beq v0,zero,800a2ef8 */
    v1 = RD16(s0 + 0x8u);                       /* lhu v1,0x8(s0) */
    v0 = (v1 < 2u) ? 1u : 0u;                   /* sltiu v0,v1,2 */
    t = v0;
    v0 = v1 + 1u;                               /* 800a2eec (delay slot of beq at 2ee8) */
    if (t == 0) goto L_2F20;                    /* beq v0,zero,800a2f20 */
    WR16(s0 + 0x8u, v0);                        /* 800a2ef4 (delay slot of j 800a2f14) */
    goto L_2F14;

L_2EF8:
    /* 800a2ef8: v0 = v1 & 0x10 */
    if (v0 == 0) goto L_2F20;                   /* beq v0,zero,800a2f20 */
    v0 = RD16(s0 + 0x8u);                       /* lhu v0,0x8(s0) */
    t = v0;
    v0 = v0 - 1u;                               /* 800a2f0c (delay slot of beq at 2f08) */
    if (t == 0) goto L_2F20;                    /* beq v0,zero,800a2f20 (tests the loaded value) */
    WR16(s0 + 0x8u, v0);                        /* sh v0,0x8(s0) */

L_2F14:
    /* 800a2f14 */
    a0 = 137u;                                  /* addiu a0,zero,137 */

L_2F18:
    /* 800a2f18 */
    v0 = m->call4(m->ctx, 0x8002438Cu, a0, a1, a2, a3);   /* jal 8002438c (delay slot nop) */

L_2F20:
    /* 800a2f20 */
    v0 = m->call4(m->ctx, 0x800A304Cu, s0, a1, a2, a3);   /* jal 800a304c (delay slot addu a0,s0,0) */
    m->sp_set(m->ctx, sp + 0x28u);              /* restore sp (addiu sp,sp,40 at 800a2f34) */
    return v0;
}
