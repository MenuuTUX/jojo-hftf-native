#include "manual.h"
#include "hex2c_rt.h"

/* PL01X 800FC1D0..800FC270 (slot B, base 800F4800). $a0 = P. $a1 is loaded (lw a1,0x80(a0)) before any read, so it is a
 * dummy parameter (nargs 4 keeps $a2 and $a3 as parameters 3 and 4). $a2/$a3 are not written before the jal, so the
 * jal 800268bc passes the entry values. $a1 at the jal is the word 0x80103494. The return is $v0 of 800268bc.
 * Q = word P+0x80. The branch on byte P+0x0B picks the sign (+ or -) of the table byte at (byte P+4) + 0x800FF8DC. The
 * delay slot `sb zero,1(a0)` runs on both paths. */
uint32_t man_PL01X_800FC1D0(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;
    uint32_t q, tbl;
    uint32_t sum;
    int32_t off;
    (void)a1;
    WR8(p + 0xB8u, 0x80u);                              /* addiu v0,zero,128; sb v0,0xb8(a0) */
    {
        uint8_t flag = RD8(p + 0xBu);                   /* lbu v0,0xb(a0) (800fc1e0) */
        q = RD32(p + 0x80u);                            /* lw a1,0x80(a0) (800fc1e4) */
        WR32(p + 0x108u, 0u);                          /* sw zero,0x108(a0) */
        WR8(p + 1u, 0u);                                /* delay slot of beq at 800fc1ec, both paths */
        if (flag != 0u) {                               /* fallthrough: beq not taken */
            off = (int32_t)(int8_t)RD8(RD8(p + 4u) + 0x800FF8DCu);  /* lbu v1,4(a0); lbu v0,0(v1) = table byte */
            sum = RD16(q + 0x6Au) + (uint32_t)off;      /* lhu v1,0x6a(a1); addu v1,v1,v0 (delay of j 800fc240) */
        } else {                                        /* beq taken to 800fc21c */
            off = (int32_t)(int8_t)RD8(RD8(p + 4u) + 0x800FF8DCu);  /* lbu v1,4(a0); lbu v0,0(v1) */
            sum = RD16(q + 0x6Au) - (uint32_t)off;      /* lhu v1,0x6a(a1); subu v1,v1,v0 */
        }
        WR16(p + 0x6Au, sum);                           /* sh v1,0x6a(a0) at 800fc240 */
    }
    WR16(p + 0x6Eu, RD16(q + 0x6Eu) + 64u);             /* lhu v0,0x6e(a1); addiu v0,v0,64; sh v0,0x6e(a0) */
    tbl = RD32(0x80103494u);                            /* lui v0,0x8010; lw a1,0x3494(v0) (800fc258) */
    return m->call4(m->ctx, 0x800268BCu, p, tbl, a2, a3); /* jal 800268bc, nop delay slot */
}
