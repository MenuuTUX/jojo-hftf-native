#include "manual.h"
#include "hex2c_rt.h"

/* 800E4BAC (PL16, slot A, 300 bytes). $a0 = P, $a1 = n. $a2 is overwritten (addu a2,a0,zero) before any read, so the
 * function reads only $a0 and $a1 (nargs 2). There are no calls. $a3 is written (mfhi a3) before any read.
 * Dispatch: v3 = n - 1; if (v3 < 7) jump through the word at 0x800DF0F4 + 4*v3 (lui 800e; addiu -3852), else return 0
 * (the beq delay slot and the sltiu leave $v0 = 0 on that path). The words select three bodies: entries 0-1 (n = 1, 2),
 * entries 2-3 (n = 3, 4) and entries 4-6 (n = 5..7). Each body divides P+0x70 and P+0x74 by 5 with the signed magic
 * multiply (mult by 0x66666667, mfhi, sra 1, minus the sign), adds 1, 3 or 4 times that quotient to the word, and
 * returns $v0 = the quotient of P+0x74 times 1, 3 or 4 (the value left by the last subu/sll before the j 800e4cc8). */

/* Signed x / 5 exactly as the mult/mfhi/sra/subu sequence computes it. */
static int32_t mips_div5(int32_t x) {
    int32_t hi = (int32_t)(((int64_t)x * (int64_t)0x66666667) >> 32);   /* mfhi of mult x, 0x66666667 */
    return (hi >> 1) - (x >> 31);
}

uint32_t man_PL16_800E4BAC(const GuestMem *m, uint32_t a0, uint32_t a1) {
    uint32_t p = a0;                                /* addu a2,a0,zero: P is kept in a2 */
    uint32_t v3 = a1 - 1u;                          /* addiu v3,a1,-1 */
    uint32_t tbl, q0, q1, a1w, a0w;

    if (!(v3 < 7u)) return 0u;                      /* sltiu v0,v3,7; beq v0,zero,800e4cd0 */
    tbl = RD32(0x800DF0F4u + 4u * v3);              /* sll v3,v3,2; addu; lw v0,0(v3); jr v0 */

    if (tbl == 0x800E4BDCu) {                       /* entries 0, 1 (n = 1, 2) */
        a0w = RD32(p + 0x70u);                      /* lw a0,0x70(a2) */
        q0 = (uint32_t)mips_div5((int32_t)a0w);
        WR32(p + 0x70u, a0w + q0);                  /* addu a0,a0,v0; sw a0,0x70(a2) */
        a1w = RD32(p + 0x74u);                      /* lw a1,0x74(a2) */
        q1 = (uint32_t)mips_div5((int32_t)a1w);     /* a3 = hi(a1 * M); v0 = a3 >> 1 - sign(a1) */
        WR32(p + 0x74u, a1w + q1);                  /* addu a1,a1,v0; sw a1,0x74(a2) */
        return q1;                                  /* $v0 at jr ra (delay slot subu v0,v0,v3) */
    }
    if (tbl == 0x800E4C24u) {                       /* entries 2, 3 (n = 3, 4) */
        a0w = RD32(p + 0x70u);
        q0 = (uint32_t)mips_div5((int32_t)a0w);
        WR32(p + 0x70u, a0w + 3u * q0);             /* v0 = 3 * q0 */
        a1w = RD32(p + 0x74u);
        q1 = (uint32_t)mips_div5((int32_t)a1w);
        WR32(p + 0x74u, a1w + 3u * q1);             /* addu a1,a1,v0 (v0 = 3*q1) */
        return 3u * q1;
    }
    /* entries 4, 5, 6 (n = 5, 6, 7) */
    a0w = RD32(p + 0x70u);
    q0 = (uint32_t)mips_div5((int32_t)a0w);
    WR32(p + 0x70u, a0w + 4u * q0);                 /* v0 = 4 * q0 */
    a1w = RD32(p + 0x74u);
    q1 = (uint32_t)mips_div5((int32_t)a1w);
    WR32(p + 0x74u, a1w + 4u * q1);                 /* v0 = 4 * q1 */
    return 4u * q1;
}
