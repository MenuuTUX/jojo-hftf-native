#include "manual.h"
#include "hex2c_rt.h"

/* 800193A8 (TEXT). Works on the global block S = 0x80079208 only (no stack buffer). $a0, $a1, $a2 are written before
 * any read, so they are dummy parameters here; $a3 is never written and is passed on unchanged to every callee.
 * Callees 800424E0, 80042670 and 80018EE8 get the values the original leaves in $a1, $a2 at each call (s0, byte B8). */
uint32_t man_800193A8(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t s1 = 0x80079208u;
    uint32_t s2, s3, s0, e, base, bb8, r, v0;

    v0 = (RD8(s1 + 0x82u) - RD8(s1 + 0x83u)) & 7u;      /* subu s3,v1,v0; andi s3,s3,7 */
    s3 = v0;
    if (RD8(s1 + 0x33u) != 0 && s3 == 0u)              /* lbu v0,0x33; beq; bne s3,zero; addiu s3,zero,8 */
        s3 = 8u;
    s2 = 0u;
    if ((int32_t)s3 <= 0)                               /* blez s3 at 800193f8 */
        goto L_end;

L_top:
    WR8(s1 + 0x34u, 1u);                                /* sb s4,0x34(s1) at 80019414 */
    base = 0x80111800u + (RD8(s1 + 0x83u) << 11);       /* lui a1 / ori a1 / sll a0,a0,11 / addu a1,a0,a1 */
    e = s1 + 56u * RD8(s1 + 0x81u);                     /* sll/subu/sll/addu: entry = S + 56 * idx */
    s0 = RD32(e + 0x88u);                               /* lw v0,0x88(v1) */
    if ((int32_t)s0 >= 2048)                            /* slti v0,s0,2048; bne skips the clamp */
        s0 = 2048u;
    bb8 = RD8(e + 0xB8u);                               /* lbu a2,0xb8(v1) */
    r = m->call4(m->ctx, 0x800424E0u, base, s0, bb8, a3); /* jal 800424e0 at 80019450 */

    if ((int32_t)(int16_t)(uint16_t)r == -1) {          /* sll/sra 16; beq v0,v1(-1) at 80019464 */
        WR8(s1 + 0x408u, 1u);                           /* sb v0,0x408(s1) */
        WR8(s1 + 0x45u, RD8(s1 + 0x45u) + 1u);          /* lbu v0,0x45; addiu; sb v0,0x45 */
        e = s1 + 56u * RD8(s1 + 0x81u);
        WR16(e + 0x84u, 0u);                            /* sh zero,0x84(v0): delay slot of jal 80018ee8 */
        m->call4(m->ctx, 0x80018EE8u, s1, s0, bb8, a3); /* a0 = s1 (delay slot at 80019468) */
        goto L_end;
    }

    do {                                                /* jal 80042670 with a0 = 0; loop while low half is 0 */
        r = m->call4(m->ctx, 0x80042670u, 0u, s0, bb8, a3);
    } while ((uint16_t)r == 0u);

    e = s1 + 56u * RD8(s1 + 0x81u);
    WR32(e + 0x88u, RD32(e + 0x88u) - s0);              /* lw v1,0x88; subu v1,v1,s0; sw v1,0x88(v0) */
    WR8(s1 + 0x83u, (RD8(s1 + 0x83u) + 1u) & 7u);      /* lbu v0,0x83; addiu; andi 7; sb */
    e = s1 + 56u * RD8(s1 + 0x81u);
    if ((int32_t)RD32(e + 0x88u) > 0) {                 /* lw v0,0x88(v1); bgtz v0 at 800194d4 */
        s2 = s2 + 1u;                                   /* addiu s2,s2,1 at 8001950c */
        WR16(s1 + 0x42u, 0u);                           /* sh zero,0x42(s1): delay slot of bne at 80019514 */
        if ((int32_t)s2 < (int32_t)s3)                  /* slt v0,s2,s3; bne back to 80019404 */
            goto L_top;
        goto L_end;
    }

    WR16(e + 0x84u, 0u);                                /* sh zero,0x84(v1) at 800194dc */
    v0 = RD8(s1 + 0x81u) + 1u;                          /* lbu v0,0x81; addiu v0,v0,1 */
    WR8(s1 + 0x81u, v0);                                /* sb v0,0x81(s1): delay slot of bne at 800194ec */
    if (RD8(s1 + 0x59u) != 0)                           /* lbu v1,0x59; bne v1,zero,8001951c */
        goto L_end;
    if ((v0 & 0xFFu) == RD8(s1 + 0x80u))                /* andi v0,v0,0xff; lbu v1,0x80; bne */
        WR8(s1 + 0x3Fu, 1u);                            /* sb s4,0x3f(s1): delay slot of j 8001951c */

L_end:
    if (s3 == s2)                                       /* bne s3,s2,80019564 */
        WR8(s1 + 0x34u, 0u);                            /* sb zero,0x34(s1) in the delay slot of j 80019568 */
    else
        WR8(s1 + 0x34u, 1u);                            /* sb v0,0x34(s1), v0 = 1 (delay slot at 80019520) */
    return 1u;
}
