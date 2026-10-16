#include "manual.h"
#include "hex2c_rt.h"

/* 800A25C4 (GAMESS), 8 bytes: lui v0,0x8006; lbu v0,0x6c6b(v0). It falls through into 800A25CC (428 bytes, frame 0x18),
 * so both are written here as one unit. The stub sets $v0 (no $v0 input). Registers read before written: $a0 (kept as s0),
 * and $a1 (the store at 800a26e0). $a2 and $a3 are never written, so every jal (8002438C) passes them on unchanged: nargs 4.
 * The saved s0/ra slots need no code. Returns the $v0 at jr ra. */
uint32_t man_800A25C4(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp, s0, v0, v1, t;

    sp = m->sp_get(m->ctx) - 0x18u;             /* addiu sp,sp,-24 */
    m->sp_set(m->ctx, sp);
    s0 = a0;                                    /* addu s0,a0,zero */

    v0 = RD8(0x80066C6Bu);                      /* stub: lbu v0,0x6c6b(v0), v0 = 0x80060000 */
    v0 &= 1u;                                   /* andi v0,v0,1 */
    if (v0 == 0) {
        v1 = RD16(0x8006CECEu);                 /* lui v0,0x8007; lhu v1,-0x3132(v0) */
    } else {
        v1 = RD16(0x8006CEBEu);                 /* lui v0,0x8007; lhu v1,-0x3142(v0) */
    }

    /* 800a25fc */
    t = RD32(s0 + 0xcu);                        /* lw v0,0xc(s0) */
    v0 = v1 & 0x40u;                            /* 800a2608 (delay slot of beq at 2604) */
    if (t == 0) goto L_26A8;                    /* 800a2604 beq v0,zero,800a26a8 (tests the loaded value) */
    v0 = v1 & 0x10u;                            /* 800a2610 (delay slot of beq at 260c) */
    if ((v1 & 0x40u) == 0) goto L_2640;         /* 800a260c beq v0,zero,800a2640 (tests v1 & 0x40) */

    /* 800a2614 */
    v0 = (uint32_t)RDS16(s0 + 0xcu);            /* lh v0,0xc(s0) */
    v1 = RD16(s0 + 0xcu);                       /* lhu v1,0xc(s0) */
    a0 = 206u;                                  /* 800a2620 (delay slot of beq at 261c) */
    if (v0 == 0) goto L_26E4;                   /* 800a261c beq v0,zero,800a26e4 */
    v0 = v1 - 1u;                               /* addiu v0,v1,-1 */
    WR16(s0 + 0xcu, v0);                        /* sh v0,0xc(s0) */
    v0 = 59u;                                   /* addiu v0,zero,59 */
    WR16(s0 + 0xeu, v0);                        /* 800a2634 (delay slot of jal 8002438c) */
    v0 = m->call4(m->ctx, 0x8002438Cu, a0, a1, a2, a3);
    goto L_26E4;                                /* 800a2638 j 800a26e4 */

L_2640:
    /* 800a2640 (v0 = v1 & 0x10) */
    a0 = 141u;                                  /* 800a2644 (delay slot of beq at 2640) */
    if (v0 == 0) goto L_2664;                   /* 800a2640 beq v0,zero,800a2664 */
    v0 = 1u;                                    /* addiu v0,zero,1 */
    WR16(s0 + 0xcu, 0u);                        /* sh zero,0xc(s0) */
    WR16(s0 + 0xeu, 0u);                        /* sh zero,0xe(s0) */
    WR16(s0 + 0x10u, v0);                       /* 800a2658 (delay slot of jal 8002438c) */
    v0 = m->call4(m->ctx, 0x8002438Cu, a0, a1, a2, a3);
    goto L_26E4;                                /* 800a2660 j 800a26e4 */

L_2664:
    /* 800a2664 */
    v0 = RD16(s0 + 0xeu);                       /* lhu v0,0xe(s0) */
    v0 = v0 - 1u;                               /* addiu v0,v0,-1 */
    WR16(s0 + 0xeu, v0);                        /* sh v0,0xe(s0) */
    v0 = v0 << 16;                              /* sll v0,v0,16 */
    if (v0 != 0) goto L_26E4;                   /* bne v0,zero,800a26e4 */
    a0 = 206u;                                  /* 800a2684 (delay slot of jal 8002438c) */
    v0 = m->call4(m->ctx, 0x8002438Cu, a0, a1, a2, a3);
    t = (uint32_t)RDS16(s0 + 0xcu);             /* lh v0,0xc(s0) */
    v1 = RD16(s0 + 0xcu);                       /* lhu v1,0xc(s0) */
    v0 = v1 - 1u;                               /* 800a2694 (delay slot of beq at 2690) */
    if (t == 0) goto L_26E4;                    /* beq v0,zero,800a26e4 (tests the loaded value) */
    WR16(s0 + 0xcu, v0);                        /* sh v0,0xc(s0) */
    v0 = 59u;                                   /* addiu v0,zero,59 */
    WR16(s0 + 0xeu, v0);                        /* 800a26a4 (delay slot of j 800a26e4) */
    goto L_26E4;

L_26A8:
    /* 800a26a8 */
    v0 = RD16(s0 + 0x10u);                      /* lhu v0,0x10(s0) */
    v0 = v0 - 1u;                               /* addiu v0,v0,-1 */
    WR16(s0 + 0x10u, v0);                       /* sh v0,0x10(s0) */
    v0 = v0 << 16;                              /* sll v0,v0,16 */
    v1 = 2u;                                    /* 800a26c0 (delay slot of bne at 26bc) */
    if (v0 != 0) goto L_26E4;                   /* bne v0,zero,800a26e4 */
    v0 = RD16(s0 + 0x2u);                       /* lhu v0,0x2(s0) */
    WR16(s0 + 0x4u, v1);                        /* sh v1,0x4(s0) */
    WR16(s0 + 0x6u, 0u);                        /* sh zero,0x6(s0) */
    WR8(s0 + 0x22u, 0u);                        /* sb zero,0x22(s0) */
    v0 = v0 + 1u;                               /* addiu v0,v0,1 */
    WR16(s0 + 0x2u, v0);                        /* sh v0,0x2(s0) */
    WR8(a1 + 0x14u, v1);                        /* 800a26e0 (delay slot of j 800a2768) */
    goto L_2768;

L_26E4:
    /* 800a26e4 */
    v0 = RD8(s0 + 0x23u);                       /* lbu v0,0x23(s0) */
    v0 &= 1u;                                   /* andi v0,v0,1 */
    t = v0;
    v0 = 0x80070000u;                           /* lui v0,0x8007 (delay slot of beq at 26f0) */
    if (t == 0) goto L_270C;                    /* beq v0,zero,800a270c */
    v0 = RD16(0x8006CEBEu);                     /* lhu v0,-0x3142(v0) */
    v0 &= 0x800u;                               /* andi v0,v0,0x800 */
    t = v0;
    v0 = 1u;                                    /* addiu v0,zero,1 (delay slot of bne at 2704) */
    if (t != 0) goto L_2734;                    /* bne v0,zero,800a2734 (tests v0 & 0x800) */

L_270C:
    /* 800a270c */
    v0 = RD8(s0 + 0x23u);                       /* lbu v0,0x23(s0) */
    v0 &= 2u;                                   /* andi v0,v0,0x2 */
    t = v0;
    v0 = 0x80070000u;                           /* lui v0,0x8007 (delay slot of beq at 2718) */
    if (t == 0) goto L_2738;                    /* beq v0,zero,800a2738 */
    v0 = RD16(0x8006CECEu);                     /* lhu v0,-0x3132(v0) */
    v0 &= 0x800u;                               /* andi v0,v0,0x800 */
    t = v0;
    v0 = 2u;                                    /* addiu v0,zero,2 (delay slot of beq at 272c) */
    if (t == 0) goto L_2738;                    /* beq v0,zero,800a2738 */

L_2734:
    /* 800a2734 */
    WR8(s0 + 0x22u, v0);                        /* sb v0,0x22(s0) */

L_2738:
    /* 800a2738 */
    v0 = (uint32_t)RDS8(s0 + 0x22u);            /* lb v0,0x22(s0) */
    t = v0;
    a0 = 138u;                                  /* addiu a0,zero,138 (delay slot of beq at 2740) */
    if (t == 0) goto L_2768;                    /* beq v0,zero,800a2768 */
    v0 = 2u;                                    /* addiu v0,zero,2 */
    WR16(s0 + 0x4u, v0);                        /* sh v0,0x4(s0) */
    v0 = RD16(s0 + 0x2u);                       /* lhu v0,0x2(s0) */
    v1 = 1u;                                    /* addiu v1,zero,1 */
    WR16(s0 + 0x6u, v1);                        /* sh v1,0x6(s0) */
    v0 = v0 + 1u;                               /* addiu v0,v0,1 */
    WR16(s0 + 0x2u, v0);                        /* 800a2764 (delay slot of jal 8002438c) */
    v0 = m->call4(m->ctx, 0x8002438Cu, a0, a1, a2, a3);

L_2768:
    /* 800a2768: addiu sp,sp,24 after the restore of ra/s0 */
    m->sp_set(m->ctx, sp + 0x18u);
    return v0;
}
