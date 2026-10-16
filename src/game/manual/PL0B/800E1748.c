#include "manual.h"
#include "hex2c_rt.h"

/* PL0B 800E1748..800E1878 (slot A, base 800DF000). Leaf: no calls. $a0 = P (kept in $a3), $a1 = B, used before and after
 * the `addiu a1,a1,-3` at 800e178c. $a2 = Q = lw 0x170(P). $t0 = R = lw 0x130(P).
 * The index is the sign-extended byte of (B - 3). If it is not below 16 (sltiu), the beq at 800e17d0 leaves with the
 * delay slot `lui v0,0x800e` already run, so $v0 = 0x800E0000. Otherwise `jr` goes to table[idx] at 0x800DF000 (the
 * module's first 16 words): 800e17f4 (idx 0, 6, 10), 800e17fc (idx 12), 800e1818 (idx 15), 800e1838 (idx 2) or 800e1878
 * (idx 1, 3, 4, 5, 7, 8, 9, 11, 13, 14). The 800e1878 exit returns the table word, 0x800E1878, as $v0.
 * Every other path ends with $v0 = byte P+0x0B. The unaligned lwl/lwr and swl/swr pairs copy the 4 bytes at +0x68 and
 * +0x6C from P to Q, done here as byte accesses (little-endian, the same bytes in the same order). */

static void copy4(const GuestMem *m, uint32_t dst, uint32_t src) {
    uint32_t w = (uint32_t)RD8(src) | ((uint32_t)RD8(src + 1u) << 8) |
                 ((uint32_t)RD8(src + 2u) << 16) | ((uint32_t)RD8(src + 3u) << 24);
    WR8(dst, w);
    WR8(dst + 1u, w >> 8);
    WR8(dst + 2u, w >> 16);
    WR8(dst + 3u, w >> 24);
}

uint32_t man_PL0B_800E1748(const GuestMem *m, uint32_t a0, uint32_t a1) {
    uint32_t p = a0;                                    /* addu a3,a0,zero */
    uint32_t q, r, v0, t, idx;
    uint8_t v1 = RD8(p + 0x1F6u);                       /* lbu v1,0x1f6(a3) */
    q = RD32(p + 0x170u);                               /* lw a2,0x170(a3) */
    r = RD32(p + 0x130u);                               /* lw t0,0x130(a3) */

    /* P-side header stores (800e1760..800e1780). $a0 = 8 is stored to 0x1fa; $v0 = 1 to +5. */
    WR8(p + 0x1FAu, 8u);
    WR8(p + 5u, 1u);
    WR8(p + 6u, 0u);
    WR8(p + 8u, 0u);
    WR8(p + 7u, 0u);
    WR8(p + 0x206u, a1);
    WR8(p + 0x205u, 0u);
    WR8(p + 0x1FBu, 0u);
    WR8(p + 0xBu, v1);

    /* Q-side header stores (800e1784..800e17b0). $a1 is still the entry value here, and is reduced by 3 after 800e1788. */
    v1 = RD8(q + 0x1F6u);                               /* lbu v1,0x1f6(a2) */
    WR8(q + 0x206u, a1);
    a1 = a1 - 3u;                                       /* addiu a1,a1,-3 */
    WR8(q + 0x1FAu, 8u);
    WR8(q + 5u, 1u);
    WR8(q + 6u, 0u);
    WR8(q + 8u, 0u);
    WR8(q + 7u, 0u);
    WR8(q + 0x205u, 0u);
    WR8(q + 0x1FBu, 0u);
    WR8(q + 1u, 0u);
    WR8(q + 0xBu, v1);

    v0 = RD8(p + 0x208u);                               /* lbu v0,0x208(a3) */
    WR8(p + 0x207u, v0);                                /* sb v0,0x207(a3) */
    WR8(q + 0x207u, v0);                                /* sb v1,0x207(a2), with v1 = v0 */

    idx = (uint32_t)(int32_t)(int8_t)(uint8_t)(a1 & 0xFFu);  /* sll a1,a1,24; sra v1,a1,24 */
    if (idx >= 16u)                                     /* sltiu v0,v1,16; beq v0,zero,800e1878 */
        return 0x800E0000u;                             /* the delay slot lui v0,0x800e runs on this path */

    t = RD32(0x800DF000u + 4u * idx);                   /* lw v0,0(v1); jr v0 */
    if (t == 0x800E1878u)
        return t;                                       /* the exit with $v0 = the table word */

    if (t == 0x800E17F4u)
        WR8(r + 0x20u, 1u);                             /* 800e17f4: addiu v0,zero,1; sb v0,0x20(t0) */
    if (t == 0x800E17F4u || t == 0x800E17FCu) {
        v0 = RD8(p + 0x207u);                           /* 800e17fc: lbu v0,0x207(a3) */
        WR8(q + 0x207u, v0);                            /* sb v0,0x207(a2) */
        v0 = 1u;                                        /* addiu v0,zero,1 */
        WR8(p + 0x1FBu, 0u);                            /* sb zero,0x1fb(a3) */
        WR8(q + 0x1FBu, 0u);                            /* delay slot of j 800e1830 */
    } else if (t == 0x800E1818u) {
        v0 = 2u;                                        /* 800e1818 */
        WR8(q + 0x207u, v0);
        WR8(p + 0x207u, v0);
        v0 = 1u;
        WR8(p + 0x1FBu, v0);
        WR8(q + 0x1FBu, v0);
    }
    if (t != 0x800E1838u) {                             /* 800e1830: the paths that reach it (not the direct 800e1838) */
        WR8(p + 0x205u, v0);
        WR8(q + 0x205u, v0);
    }

    /* 800e1838..800e186c: copy 8 bytes of P+0x68..0x6F to Q, then the two single bytes */
    copy4(m, q + 0x68u, p + 0x68u);
    copy4(m, q + 0x6Cu, p + 0x6Cu);
    WR8(q + 0x1F6u, RD8(p + 0x1F6u));                   /* lbu v0,0x1f6(a3); sb v0,0x1f6(a2) */
    v0 = RD8(p + 0xBu);                                 /* lbu v0,0xb(a3) */
    WR8(q + 0xBu, v0);                                  /* sb v0,0xb(a2) */
    return v0;                                          /* jr ra at 800e1878 */
}
