#include "manual.h"
#include "hex2c_rt.h"

/* PL14X 800FAED0..800FB30C (slot B). $a0 = P (struct, s0). s1 = P+0x80 pointer, s2 = RD32(s1+0x17C), read at entry.
 * Every call here is followed by LEFT(): the $a1..$a3 the callee left (m->exit_reg), which the next call may pass on.
 * Calls with an explicit argument set it before the jal. The table at 0x800F482C is read as halfwords. Branch delay
 * slots run on both paths: "addiu v0,zero,1" before the P[6] test, "addiu v0,a0,1" after the P[8] test and after the
 * 0x4E-like byte compares, "addiu v0,zero,2" before the a0 != 1 test, and the sh/sb in each delay slot as listed.
 * The function ends in one of several $v0 values (see each return). Labels follow the MIPS branch targets. */
uint32_t man_PL14X_800FAED0(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;                                    /* s0 */
    uint32_t s1 = RD32(p + 0x80u);                      /* lw s1,0x80(s0) */
    uint32_t v3 = RD8(p + 0x06u);                       /* lbu v1,0x6(s0) */
    uint32_t s2 = RD32(s1 + 0x17Cu);                    /* lw s2,0x17c(s1) */
    uint32_t a1c = a1, a2c = a2, a3c = a3;              /* $a1..$a3 at each jal */
    uint32_t r, v0, v1, a0v, h, x1, x2, c, ee, d, tbl = 0x800F482Cu;

    if (v3 != 0u) {                                     /* bne v1,zero,800fb2c8; delay v0 = 1 */
        if (v3 != 1u)
            return 1u;                                  /* bne v1,v0(=1),800fb2fc */
        m->call4(m->ctx, 0x80026A04u, p, a1c, a2c, a3c); /* jal 80026a04, a0 = s0 */
        a1c = m->exit_reg(m->ctx, 5); a2c = m->exit_reg(m->ctx, 6); a3c = m->exit_reg(m->ctx, 7);
        if (RDS8(p + 0x4Du) != -1)                      /* lb v1,0x4d(s0); bne v1,v0(=-1), v0 = -1 */
            return 0xFFFFFFFFu;
        goto L_fb2e8;
    }

    /* 800FAEF8: jal 800fbcc4 (a0 = P; a1..a3 entry values, nothing called before) */
    r = m->call4(m->ctx, 0x800FBCC4u, p, a1c, a2c, a3c);
    a1c = m->exit_reg(m->ctx, 5); a2c = m->exit_reg(m->ctx, 6); a3c = m->exit_reg(m->ctx, 7);
    if (r != 0u)                                        /* bne v0,zero,800fb2e8; delay a0 = P */
        goto L_fb2e8;
    if ((RD32(s1 + 0x04u) & 0x00FFFF00u) != 0x100u)     /* lw v1,4(s1); and v1,v1,0x00ffff00; bne v1,0x100 */
        goto L_fb2e8;
    if (RD8(p + 0x139u) != 0u)                          /* lbu v0,0x139(s0); bne v0,zero,800faf4c */
        goto L_faf4c;
    if ((int32_t)RDS16(p + 0x6Eu) < 36)                 /* lh v0,0x6e(s0); slti v0,v0,36; beq v0,zero,800fafb8 */
        goto L_faf4c;
    goto L_fafb8;

L_faf4c:
    /* 800FAF4C: a0 = 2, a1 = 3; sb P[6]+1 in the delay slot of jal 8002b450 */
    WR8(p + 0x06u, RD8(p + 0x06u) + 1u);
    m->call4(m->ctx, 0x8002B450u, 2u, 3u, a2c, a3c);
    a1c = m->exit_reg(m->ctx, 5); a2c = m->exit_reg(m->ctx, 6); a3c = m->exit_reg(m->ctx, 7);
    WR8(p + 0x32u, 12u);
    WR16(p + 0x0Cu, 16u);
    WR8(p + 0xF8u, 0u);
    WR16(p + 0xF0u, 79u);
    WR16(p + 0xEEu, 79u);
    v1 = (uint32_t)(int32_t)RDS8(s1 + 0x136u);          /* lb v1,0x136(s1) */
    WR16(p + 0x1Eu, v1 * 24u + 223u);                   /* ((v1*2 + v1) << 3) + 223 */
    a1c = RD32(0x800FE634u);                            /* lui v0,0x8010; lw a1,-0x19cc(v0) */
    return m->call4(m->ctx, 0x800268BCu, p, a1c, a2c, a3c); /* jal 800268bc, a0 = s0 */

L_fafb8:
    /* 800FAFB8: lbu v0,0x6ca2(v0) with v0 = 0x80060000 (lui in the beq delay slot) */
    v0 = RD8(0x80066CA2u);
    if (v0 != 0u)                                       /* bne v0,zero,800fb2fc */
        return v0;
    a0v = RD8(p + 0x07u);                               /* lbu a0,7(s0) */
    if ((a0v & 0xFFu) != 0u)                            /* andi v1,a0,0xff; bne v1,zero,800fb048; delay v0 = 1 */
        goto L_fb048;
    /* 800FAFDC: lbu v0,8(s0); bne v0,zero,800faffc; delay v0 = a0 + 1 */
    if (RD8(p + 0x08u) != 0u)
        goto L_faffc;
    WR8(p + 0x07u, a0v + 1u);                           /* sb v0,7(s0) */
    WR16(p + 0x0Cu, 12u);                               /* addiu v0,zero,12; j 800fb2fc; sh v0,0xc(s0) */
    return 12u;

L_faffc:
    /* lhu P[0xee] += 32; lhu P[0xf0] + 32 stored in the delay slot of the bne */
    ee = (RD16(p + 0xEEu) + 32u) & 0xFFFFu;
    WR16(p + 0xEEu, ee);
    v0 = RD16(p + 0xF0u) + 32u;
    WR16(p + 0xF0u, v0);                                /* sh v0,0xf0(s0) in the delay slot, both paths */
    if (ee < 63u)                                       /* sltiu v1,v1,63; bne v1,zero,800fb2fc */
        return v0;
    WR16(p + 0x0Cu, 2u);                                /* addiu v0,zero,2; sh v0,0xc(s0) */
    v0 = RD8(p + 0x07u);                                /* lbu v0,7(s0) */
    WR16(p + 0xF0u, 63u);
    WR16(p + 0xEEu, 63u);
    v0 = v0 + 1u;
    WR8(p + 0x07u, v0);                                 /* j 800fb2fc; sb v0,7(s0) in the delay slot */
    return v0;

L_fb048:
    /* 800FB048: v0 = 1 (delay of bne); bne v1,v0,800fb140; delay v0 = 2 */
    if (a0v != 1u)
        goto L_fb140;
    /* 800FB050: P[0xc] -= 1; if sext16(P[0xc]) != -1, return it */
    c = (RD16(p + 0x0Cu) - 1u) & 0xFFFFu;
    WR16(p + 0x0Cu, c);
    v0 = (uint32_t)(int32_t)(int16_t)c;
    if ((int32_t)v0 != -1)                              /* bne v0,v1(=-1),800fb2fc */
        return v0;
    /* 800FB070: jal 80026afc, a0 = P */
    m->call4(m->ctx, 0x80026AFCu, p, a1c, a2c, a3c);
    a1c = m->exit_reg(m->ctx, 5); a2c = m->exit_reg(m->ctx, 6); a3c = m->exit_reg(m->ctx, 7);
    /* 800FB078: table at 0x800F482C; lh P[0x7a] indexes it (word entries, halfword reads) */
    h = (uint32_t)(int32_t)RDS16(p + 0x7Au);
    x1 = RD16(tbl + 4u * h);                            /* lhu v1,0(v1) with v1 = tbl + 4h */
    x2 = RD16(tbl + ((4u * h) | 2u));                   /* lhu v0,0(v0) with v0 = tbl + ((h<<2)|2) */
    a0v = RD8(p + 0x08u);                               /* lbu a0,8(s0) */
    WR32(p + 0x70u, 0u);
    WR32(p + 0x74u, 0u);
    WR16(p + 0x72u, x1);
    WR16(p + 0x76u, x2);                                /* sh v0,0x76(s0): delay slot of bne a0,zero */
    if (a0v != 0u) {
        v0 = RD32(p + 0x70u);
        v0 = (uint32_t)((int32_t)v0 >> 3);
        v1 = RD32(p + 0x74u);
        v1 = (uint32_t)((int32_t)v1 >> 3);
    } else {
        v0 = RD32(p + 0x70u);
        v0 = (uint32_t)((int32_t)v0 >> 5);
        v1 = RD32(p + 0x74u);
        v1 = (uint32_t)((int32_t)v1 >> 5);
    }
    WR32(p + 0x70u, v0);
    WR32(p + 0x74u, v1);
    WR16(p + 0x0Cu, 18u);                               /* addiu v0,zero,18; sh v0,0xc(s0) */
    WR16(p + 0x0Eu, 1u);                                /* addiu v1,zero,1; sh v1,0xe(s0) */
    WR8(p + 0x07u, RD8(p + 0x07u) + 1u);                /* lbu v0,7; addiu; sb v0,7 */
    /* 800FB104: lb v1,0x137(s2); a0 = 8 when it equals 5 (delay slot), else 64 */
    a0v = (RDS8(s2 + 0x137u) == 5) ? 8u : 64u;
    WR16(p + 0x7Eu, a0v);                               /* sh a0,0x7e(s0) */
    a1c = 0u;                                           /* addu a1,zero,zero */
    a0v = (uint32_t)(int32_t)RDS8(s1 + 0x136u) + 4u;    /* lb a0,0x136(s1); jal delay addiu a0,a0,4 */
    a2c = 6u;                                           /* addiu a2,zero,6 */
    r = m->call4(m->ctx, 0x80025548u, a0v, a1c, a2c, a3c);
    if (r != 0u)                                        /* beq v0,zero,800fb2fc (return 0) */
        WR32(r + 0x80u, p);                             /* j 800fb2fc; sw s0,0x80(v0) in the delay slot */
    return r;

L_fb140:
    /* 800FB140: v0 = 2 (delay of bne at 800fb048); bne v1,v0,800fb2a4 with v1 = a0 & 0xff */
    if (a0v != 2u)
        goto L_fb2a4;
    /* 800FB148: P[0xc] -= 1; beq sext16 == -1 -> 800fb294 */
    c = (RD16(p + 0x0Cu) - 1u) & 0xFFFFu;
    WR16(p + 0x0Cu, c);
    if ((int32_t)(int16_t)c == -1)
        goto L_fb294;
    /* 800FB168: lh v0,0xe(s0); lhu v1,0xe(s0); beq v0,zero,800fb188 (delay v0 = v1 - 1) */
    v0 = (int32_t)RDS16(p + 0x0Eu);
    v1 = RD16(p + 0x0Eu);
    d = (v1 - 1u) & 0xFFFFFFFFu;
    if ((int32_t)v0 == 0)
        goto L_fb188;
    WR16(p + 0x0Eu, d);                                 /* sh v0,0xe(s0) */
    if ((d & 0xFFFFu) != 0u)                            /* sll v0,v0,16; bne v0,zero,800fb2a4 */
        goto L_fb2a4;

L_fb188:
    /* 800FB188: jal 80026630 with a0 = P[0x6a], a1 = P[0x6e], a2 = s2[0x6a], a3 = P[0x7e] + s2[0x6e] */
    a0v = (uint32_t)(int32_t)RDS16(p + 0x6Au);
    a1c = (uint32_t)(int32_t)RDS16(p + 0x6Eu);
    v0 = (uint32_t)(int32_t)RDS16(s2 + 0x6Eu);
    a3c = (uint32_t)(int32_t)RDS16(p + 0x7Eu);
    a2c = (uint32_t)(int32_t)RDS16(s2 + 0x6Au);
    s1 = RD8(p + 0x7Au);                                /* lbu s1,0x7a(s0): s1 is a byte from here on */
    a3c = v0 + a3c;                                     /* addu a3,v0,a3 (delay slot of jal 80026630) */
    r = m->call4(m->ctx, 0x80026630u, a0v, a1c, a2c, a3c);
    /* the call above left its own $a1..$a3; the following code uses only $v0 */
    v0 = r;
    if ((s1 & 0xFFu) == (v0 & 0xFFu))                   /* sll v1,s1,24; sll v0,a0,24; beq v1,v0,800fb220 */
        goto L_fb220;
    d = (s1 - v0) << 24;                                /* subu v0,s1,a0 (delay slot); sll v0,v0,24 */
    if ((int32_t)d > 0)                                 /* blez v0,800fb1d4 */
        v0 = RD16(p + 0x7Au) - 6u;                      /* lhu v0,0x7a(s0); j 800fb1e0; addiu v0,v0,-6 */
    else
        v0 = RD16(p + 0x7Au) + 6u;                      /* 800FB1D4: lhu v0,0x7a; addiu v0,v0,6 */
    WR16(p + 0x7Au, v0);                                /* sh v0,0x7a(s0) */
    v1 = RD8(p + 0x7Au);                                /* lbu v1,0x7a(s0) */
    v0 = RD8(p + 0x0Bu);                                /* lbu v0,0xb(s0) */
    WR16(p + 0x7Au, v1);                                /* sh v1,0x7a(s0): delay slot of bne v0,zero, both paths */
    v0 = (v0 != 0u) ? 320u : 448u;                      /* 800FB200 / 800FB1FC */
    d = (v0 - v1) & 0xFFu;                              /* subu v0,v0,v1; andi v0,v0,0xff */
    WR16(p + 0x0Eu, d);                                 /* sh v0,0xe(s0) */
    WR8(p + 0xF8u, RD8(p + 0x0Eu));                     /* lbu v0,0xe(s0); sb v0,0xf8(s0) */
    WR16(p + 0x0Eu, 0u);                                /* sh zero,0xe(s0) */

L_fb220:
    /* 800FB220: table lookup again, with P[0x7a] as it now stands */
    h = (uint32_t)(int32_t)RDS16(p + 0x7Au);
    x1 = RD16(tbl + 4u * h);
    x2 = RD16(tbl + ((4u * h) | 2u));
    a0v = RD8(p + 0x08u);                               /* lbu a0,8(s0) */
    WR32(p + 0x74u, 0u);
    WR32(p + 0x70u, 0u);
    WR16(p + 0x72u, x1);
    WR16(p + 0x76u, x2);                                /* sh v0,0x76(s0) in the delay slot of bne a0,zero */
    if (a0v != 0u) {
        v0 = RD32(p + 0x70u);
        v0 = (uint32_t)((int32_t)v0 >> 3);
        v1 = RD32(p + 0x74u);
        v1 = (uint32_t)((int32_t)v1 >> 3);
    } else {
        v0 = RD32(p + 0x70u);
        v0 = (uint32_t)((int32_t)v0 >> 4);
        v1 = RD32(p + 0x74u);
        v1 = (uint32_t)((int32_t)v1 >> 4);
    }
    WR32(p + 0x70u, v0);
    WR32(p + 0x74u, v1);
    goto L_fb2a4;

L_fb294:
    /* 800FB294: lbu v0,7(s0); addiu v0,v0,1; sb v0,7(s0); falls into 800fb2a4 */
    WR8(p + 0x07u, RD8(p + 0x07u) + 1u);

L_fb2a4:
    /* 800FB2A4: P[0x68] += P[0x70]; P[0x6c] += P[0x74]; return the new P[0x68] in $v0 */
    v0 = RD32(p + 0x68u);
    a0v = RD32(p + 0x70u);
    v1 = RD32(p + 0x6Cu);
    a1c = RD32(p + 0x74u);
    v0 = v0 + a0v;
    WR32(p + 0x68u, v0);
    WR32(p + 0x6Cu, v1 + a1c);
    return v0;

L_fb2e8:
    /* 800FB2E8: P[5] += 1 (sb in the delay slot of jal 80025a50, a0 = P); a1 = 2 */
    WR8(p + 0x05u, RD8(p + 0x05u) + 1u);
    return m->call4(m->ctx, 0x80025A50u, p, 2u, a2c, a3c);

}
