#include "manual.h"
#include "hex2c_rt.h"

/* PL08X 800FC91C..800FCABC (416 bytes, slot B base 800F4800). P = a0, R = RD32(P+0x80), state byte S = RD8(P+6).
 * S == 0: sets P+6 = 1, P+1 = 1, jal 800268BC(P, RD32(0x80102C68)), jal 800BB2A4(P, 98), jal 800258F0(P, 1).
 * S == 1 (also falls out of S == 0): jal 80026A04(P); P+3 = (P+3 - 1) & 0xFF; if that is nonzero return it. Otherwise
 * P+0xC = 16, P+6 += 1, P+0x70 = ((d * 0x2E8BA2E9) >> 32 >> 2) - (d >> 31) with d = RD32(RD32(R+0x17C)+0x68) - RD32(P+0x68);
 * return that value. S == 2: jal 80026A04, jal 800B428C; P+0xC -= 1; return (P+0xC << 16) if nonzero, else P+6 += 1 and
 * return 800258F0(P, 2). S == 3: jal 80026A04, jal 800B428C; if RD8(P+0x139) == 0 and RS16(P+0x6E) >= 36 return 0;
 * otherwise P+5 += 1, jal 80025A50(P, 1) and return 80025A50(P, 2). Any other S returns 3.
 * The generated C read the words at RD32(0x80105490) and the lui pairs again; the first 800268BC argument is
 * RD32(0x80102C68). $a2 and $a3 pass through unchanged. $a1 is the caller's on the S == 1 path (GUESS after callees). */
uint32_t man_PL08X_800FC91C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0, r, v0, v3, arg1 = a1, hw, r17c, sgn, hi;
    int64_t prod;
    int32_t d;

    v3 = RD8(p + 0x06u);                            /* lbu v1,0x6(s0) */
    r = RD32(p + 0x80u);                            /* lw s1,0x80(s0) */
    if (v3 == 1u) {                                 /* beq v1,a0(=1),800fc9a0 */
        goto label9;
    }
    if (v3 < 2u) {                                  /* slti v0,v1,2; v3 is 0 here */
        WR8(p + 0x06u, 1u);                         /* sb a0(=1),0x6(s0) */
        WR8(p + 0x01u, 1u);                         /* sb a0,0x1(s0) */
        arg1 = RD32(0x80102C68u);                   /* lui v0,0x8010; lw a1,0x2c68(v0) */
        (void)m->call4(m->ctx, 0x800268BCu, p, arg1, a2, a3);   /* jal 800268bc */
        arg1 = 98u;                                 /* addiu a1,zero,98 (delay slot of the next jal) */
        (void)m->call4(m->ctx, 0x800BB2A4u, p, arg1, a2, a3);   /* jal 800bb2a4 */
        arg1 = 1u;                                  /* addiu a1,zero,1 */
        (void)m->call4(m->ctx, 0x800258F0u, p, arg1, a2, a3);   /* jal 800258f0 */
        goto label9;
    }
    if (v3 == 2u) {                                 /* beq v1,v0(=2),800fca08 */
        goto case2;
    }
    if (v3 == 3u) {                                 /* beq v1,v0(=3),800fca50 */
        goto case3;
    }
    return 3u;                                      /* j 800fcaa8 with $v0 = 3 */

case2:
    (void)m->call4(m->ctx, 0x80026A04u, p, arg1, a2, a3);   /* jal 80026a04 */
    (void)m->call4(m->ctx, 0x800B428Cu, p, arg1, a2, a3);   /* jal 800b428c */
    hw = (uint16_t)(RD16(p + 0x0Cu) - 1u);          /* lhu v0,0xc(s0); addiu v0,v0,-1; sh v0,0xc(s0) */
    WR16(p + 0x0Cu, hw);
    v0 = (uint32_t)hw << 16;                        /* sll v0,v0,16 */
    if (v0 != 0u) {                                 /* bne v0,zero,800fcaa8 */
        return v0;
    }
    WR8(p + 0x06u, RD8(p + 0x06u) + 1u);            /* lbu v0,6(a0); addiu v0,v0,1; jal 800258f0 (delay sb v0,6(a0)) */
    arg1 = 2u;                                      /* addiu a1,zero,2 */
    return m->call4(m->ctx, 0x800258F0u, p, arg1, a2, a3);   /* jal 800258f0; its $v0 is returned */

case3:
    (void)m->call4(m->ctx, 0x80026A04u, p, arg1, a2, a3);   /* jal 80026a04 */
    (void)m->call4(m->ctx, 0x800B428Cu, p, arg1, a2, a3);   /* jal 800b428c */
    if (RD8(p + 0x139u) == 0u) {                    /* lbu v0,0x139(s0); bne v0,zero,800fca84 not taken */
        if ((int32_t)RDS16(p + 0x6Eu) >= 36) {      /* lh v0,0x6e; slti v0,v0,36; beq v0,zero,800fcaa8 (v0 = 0) */
            return 0u;
        }
    }
    /* 800fca84 */
    WR8(p + 0x05u, RD8(p + 0x05u) + 1u);            /* lbu v0,5(s0); addu v0,v0,a1(=1); sb v0,5(s0) (jal delay slot) */
    arg1 = 1u;
    (void)m->call4(m->ctx, 0x80025A50u, p, arg1, a2, a3);   /* jal 80025a50 */
    arg1 = 2u;                                      /* addiu a1,zero,2 (delay slot of the next jal) */
    return m->call4(m->ctx, 0x80025A50u, p, arg1, a2, a3);   /* jal 80025a50; its $v0 is returned */

label9:
    (void)m->call4(m->ctx, 0x80026A04u, p, arg1, a2, a3);   /* 800fc9a0: jal 80026a04 */
    v0 = (RD8(p + 0x03u) - 1u) & 0xFFu;             /* lbu v0,3(s0); addiu v0,v0,-1; sb v0,3(s0); andi v0,v0,0xff */
    WR8(p + 0x03u, v0);
    if (v0 != 0u) {                                 /* bne v0,zero,800fcaa8 */
        return v0;
    }
    WR16(p + 0x0Cu, 16u);                           /* addiu v3,zero,16; sh v3,0xc(s0) */
    WR8(p + 0x06u, RD8(p + 0x06u) + 1u);            /* lbu v0,6(s0); addiu v0,v0,1; sb v0,6(s0) */
    r17c = RD32(r + 0x17Cu);                        /* lw v0,0x17c(s1) */
    d = (int32_t)(RD32(r17c + 0x68u) - RD32(p + 0x68u));   /* lw v1,0x68(v0); lw v0,0x68(s0); subu v1,v1,v0 */
    prod = (int64_t)d * (int64_t)(int32_t)0x2E8BA2E9u;     /* mult v1,a0 (a0 = lui 0x2e8b | 0xa2e9) */
    hi = (uint32_t)(int32_t)(prod >> 32);           /* mfhi a2 */
    sgn = (d < 0) ? 0xFFFFFFFFu : 0u;               /* sra v1,v1,31 */
    v0 = (uint32_t)(((int32_t)hi) >> 2) - sgn;      /* sra v0,a2,2; subu v0,v0,v1 */
    WR32(p + 0x70u, v0);                            /* j 800fcaa8; delay sw v0,0x70(s0) */
    return v0;
}
