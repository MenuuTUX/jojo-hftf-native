#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800AD604(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2);
uint32_t man_800BC51C(const GuestMem *m, uint32_t a0, uint32_t a1);

/* 800ACE90 (AC_MAIN). $a0 = A (struct), $a1 = B (struct, its fields at 0x174 and 0x178 are pointers). Frame 0x40; the word
 * at sp+0x10 is the local whose address goes to 800AD604 and is read back. $a2 and $a3 are not written before the first jal,
 * so they reach the callees unchanged: nargs is 4. $v0 is written before any read. Returns the $v0 of the path taken.
 * Registers $a0..$a3 are tracked as r0..r3 because the calls below pass them on as the original leaves them. */
uint32_t man_800ACE90(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const uint32_t S7 = 0x80066CA0u;  /* $s7 */
    const uint32_t FP = 0x80066C48u;  /* $fp */
    const uint32_t G = 0x800CD24Cu;   /* dword_800CD24C, the word at $s4 - 0x2DB4 with $s4 = 0x800D0000 */
    uint32_t sp = m->sp_get(m->ctx) - 0x40u;
    uint32_t r0 = a0, r1 = a1, r2 = a2, r3 = a3;
    uint32_t s0, s1, s2, s3, s5, s6, v0, v1;

    m->sp_set(m->ctx, sp);

    /* 800ace9c..800acedc */
    s6 = a1;
    s2 = RD32(s6 + 0x174u);
    s3 = RD32(s6 + 0x178u);
    v0 = RD8(S7 + 0x0Eu);
    s5 = a0;
    if (v0 != 0u)
        goto L_EPI;                   /* bne to 800ad274 with $v0 = that byte */

    /* 800acee4..800acf18 */
    v1 = RD8(s5 + 0x138u);
    v0 = v1 * 3u;
    v1 = RD32(s5 + 0x12Cu);
    r0 = RD8(s2 + 0x157u);
    v1 = RD32(v1 + 0x18u);
    v0 = v0 << 4;
    s1 = v1 + v0;
    if (r0 == 0u) {                   /* bne at 800acf04 skips this when $a0 != 0 */
        v0 = RD8(s1 + 0x1Fu);
        WR8(s2 + 0x157u, v0);
    }

    /* 800acf18..800acf38 */
    s0 = (uint32_t)(int32_t)RDS8(s1 + 0x00u);
    r0 = s0;
    v0 = m->call4(m->ctx, 0x8003D650u, r0, r1, r2, r3);
    if ((int32_t)s0 < 0) {           /* bgez at 800acf24 */
        r1 = 0x43800000u;
        r0 = v0;
        v0 = m->call4(m->ctx, 0x8003D020u, r0, r1, r2, r3);
    }
    WR32(G, v0);

    /* 800acf3c..800acf5c */
    s0 = (uint32_t)(int32_t)RDS8(s1 + 0x01u);
    r0 = s0;
    v0 = m->call4(m->ctx, 0x8003D650u, r0, r1, r2, r3);
    WR32(sp + 0x10u, v0);            /* sw in the bgez delay slot at 800acf4c: runs on both outcomes */
    if ((int32_t)s0 < 0) {
        r1 = 0x43800000u;
        r0 = v0;
        v0 = m->call4(m->ctx, 0x8003D020u, r0, r1, r2, r3);
        WR32(sp + 0x10u, v0);
    }

    /* 800acf60..800acfb8 */
    v1 = (uint32_t)(int32_t)RDS8(s5 + 0x136u);
    v0 = 0x800CDD50u + 912u * v1;    /* the sll/subu/addu chain gives 912 * $v1 */
    v0 = RD8(v0 + 0x203u);
    if (v0 != 0u) {
        r0 = RD32(G);
        r1 = 0x3E800000u;
        v0 = m->call4(m->ctx, 0x8003D720u, r0, r1, r2, r3);
        r0 = v0;
        WR32(G, r0);                 /* sw in the jal delay slot */
        v0 = m->call4(m->ctx, 0x8003D3C0u, r0, r1, r2, r3);
        if (((v0 << 16) & 0x80000000u) != 0u) {   /* bgez at 800acfb4 falls through when bit 15 is set */
            v0 = 0x3F800000u;
            WR32(G, v0);
        }
    }

    /* 800acfc4..800acff8 */
    v0 = RD8(S7 + 0x02u);
    s0 = 0x800D0000u;
    if (v0 != 0u) {
        r0 = RD32(G);
        r1 = 0x3F000000u;
        v0 = m->call4(m->ctx, 0x8003D720u, r0, r1, r2, r3);
        r0 = v0;
        WR32(G, r0);                 /* sw in the jal delay slot */
        v0 = m->call4(m->ctx, 0x8003D3C0u, r0, r1, r2, r3);
        if (((v0 << 16) & 0x80000000u) != 0u) {
            v0 = 0x3F800000u;
            WR32(G, v0);
        }
    }

    /* 800ad004..800ad028 */
    v0 = (uint32_t)(int32_t)RDS8(S7 + 0x24u);
    r0 = s5;
    if (v0 == 0u) {
        r1 = s6;
        r2 = G;
        m->call4(m->ctx, 0x800AD2A4u, r0, r1, r2, r3);
        r0 = s5;
        r1 = s6;
        r2 = sp + 0x10u;
        v0 = man_800AD604(m, r0, r1, r2);
    }

    /* 800ad034..800ad040 */
    if (RD8(s2 + 0x169u) != 0u) {   /* bne at 800ad03c; its delay slot sets $v0 = 1 on both outcomes */
        v0 = 1u;
        goto L_0F4;
    }
    v0 = 1u;

    /* 800ad044..800ad088 */
    v1 = (uint32_t)(int32_t)RDS8(FP + 0x24u);
    if (v1 == 3u) {
        v0 = 0x80070000u;
        v1 = (uint32_t)(int32_t)RDS8(v0 - 0x2D15u);
        v0 = (uint32_t)(int32_t)RDS8(s6 + 0x136u);
        if (v1 == v0) {
            v0 = 0x800D0000u;
            r0 = RD32(sp + 0x10u);
            v0 = m->call4(m->ctx, 0x8003D3C0u, r0, r1, r2, r3);
            v1 = RD16(s3 + 0x16Au) - v0;
            WR16(s3 + 0x16Au, v1);
            goto L_EPI;
        }
    }

    /* 800ad08c..800ad0c8 */
    r0 = RD32(G);
    v0 = m->call4(m->ctx, 0x8003D3C0u, r0, r1, r2, r3);
    v1 = RD16(s2 + 0x16Au);
    r0 = RD32(sp + 0x10u);
    v1 = v1 - v0;
    WR16(s2 + 0x16Au, v1);
    v0 = m->call4(m->ctx, 0x8003D3C0u, r0, r1, r2, r3);
    v1 = RD16(s3 + 0x16Au) - v0;
    WR16(s3 + 0x16Au, v1);
    v0 = (uint32_t)(int32_t)RDS16(s2 + 0x16Au);
    r0 = 0x800D0000u;                /* lui in the bgez delay slot */
    if ((int32_t)v0 >= 0)
        goto L_188;

    /* 800ad0cc..800ad0e8 */
    v1 = (uint32_t)(int32_t)RDS8(FP + 0x24u);
    v0 = 3u;
    if (v1 == v0)
        goto L_0EC;
    v0 = RD8(s5 + 0x15Au);
    if (v0 == 0u)
        goto L_188;

L_0EC:
    /* 800ad0ec: j 800ad184 with sh zero in its delay slot */
    WR16(s2 + 0x16Au, 0u);
    goto L_188;

L_0F4:
    /* 800ad0f4..800ad17c; falls through to 800ad188 */
    WR8(s3 + 0x156u, v0);
    WR8(s2 + 0x156u, v0);
    v0 = 0x800D0000u;
    r0 = RD32(G);
    v0 = m->call4(m->ctx, 0x8003D3C0u, r0, r1, r2, r3);
    v1 = RD16(s2 + 0x1EEu) - v0;
    r0 = RD32(sp + 0x10u);
    WR16(s2 + 0x1EEu, v1);
    v0 = m->call4(m->ctx, 0x8003D3C0u, r0, r1, r2, r3);
    v1 = RD16(s3 + 0x1EEu) - v0;
    WR16(s3 + 0x1EEu, v1);
    v1 = (uint32_t)(int32_t)RDS8(FP + 0x24u);
    v0 = 3u;
    if (v1 == v0) {
        v0 = 0x80070000u;
        v1 = (uint32_t)(int32_t)RDS8(v0 - 0x2D15u);
        v0 = (uint32_t)(int32_t)RDS8(s6 + 0x136u);
        if (v1 == v0) {
            v0 = RD16(s2 + 0x16Au);
            WR16(s2 + 0x1EEu, v0);
            v0 = RD16(s3 + 0x16Au);
            WR16(s3 + 0x1EEu, v0);
        }
        v0 = (uint32_t)(int32_t)RDS16(s2 + 0x1EEu);
        if ((int32_t)v0 < 0)
            WR16(s2 + 0x1EEu, 0u);   /* sh zero at 800ad180 */
    }

L_188:
    /* 800ad188..800ad1f0 (a0 is 0x800D0000 on arrival; 800ad18c makes it 0x800CDD50) */
    v1 = (uint32_t)(int32_t)RDS8(s5 + 0x136u);
    r0 = 0x800CDD50u;
    r1 = 912u * v1;                  /* sll chain with sllv by 4: 57 * $v1 << 4 */
    v1 = r1 + r0;
    v0 = RD8(v1 + 0x212u);
    if (v0 != 0u) {
        v0 = 8u;
        if (RD8(v1 + 0x1FAu) == 8u)
            goto L_1F4;
    }

    /* 800ad1c8..800ad1f0 */
    v0 = 0x800CD268u;
    v1 = r1 + v0;
    v0 = RD8(v1 + 0x212u);
    if (v0 == 0u)
        goto L_220;
    v0 = 8u;
    if (RD8(v1 + 0x1FAu) != 8u)
        goto L_220;

L_1F4:
    /* 800ad1f4..800ad218 */
    v0 = (uint32_t)(int32_t)RDS8(s5 + 0x136u);
    r1 = 4u;
    r0 = 912u * v0;                  /* 57 * $v0 << 4 (sllv by $a1 = 4) */
    v0 = 0x800CDD50u;
    r0 = r0 + v0;
    v0 = m->call4(m->ctx, 0x800BC608u, r0, r1, r2, r3);

L_220:
    /* 800ad220..800ad254 */
    v1 = RD8(s5 + 0x1Au);
    v0 = 0x800D0000u;                /* lui in the bne delay slot */
    if (v1 == 2u) {
        r0 = s2;
        r1 = 26u;
        m->call4(m->ctx, 0x800BC654u, r0, r1, r2, r3);
        r0 = s2;
        r1 = 37u;
        m->call4(m->ctx, 0x800BC654u, r0, r1, r2, r3);
        r0 = s2;
        r1 = 38u;
        m->call4(m->ctx, 0x800BC654u, r0, r1, r2, r3);
    }

    /* 800ad258..800ad26c */
    r0 = RD32(G);
    v0 = m->call4(m->ctx, 0x8003D3C0u, r0, r1, r2, r3);
    r0 = s2;
    v0 = v0 << 16;
    r1 = (uint32_t)((int32_t)v0 >> 16);   /* sra in the jal delay slot */
    v0 = man_800BC51C(m, r0, r1);

L_EPI:
    m->sp_set(m->ctx, sp + 0x40u);
    return v0;
}
