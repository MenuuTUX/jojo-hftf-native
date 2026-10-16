#include "manual.h"
#include "hex2c_rt.h"

/* PL02 800DF52C..800DF65C (312 bytes, slot A base 800DF000). P = a0, Q = RD32(P+0x170), S2 = RD32(P+0x130).
 * Clears P+7, then jal 800BB2A4(P, 11). If RD8(P+0x207) != 0 and RD8(S2+0x20) == 0 this is the main path: sets P+8 = 1,
 * jal 800268BC(P, RD32(0x800EA5E4)), writes Q's record (Q+0x206 = 0, Q+0x207 = 2, then 3 if RD8(P+0x207) == 2 after a
 * jal 800268BC(P, RD32(0x800EA5E8))), copies P+0x6A/P+0x6E to Q, and ends with jal 800E1CF0(Q) whose $v0 is returned.
 * Otherwise P+8 = 0 and the return is 800268BC(P, RD32(0x800EA5D8)).
 * The generated C dereferenced each loaded word again (RD32 of the value 0x800EB0C8, 0x800EB280 or 0x800EB230, 0x800EB0C8),
 * which is the wrong argument. The lui v0,0x800f with lw -0x5a28, addiu s3,-23112 with lw 0x2c / 0x30 read 0x800EA5D8,
 * 0x800EA5E4 and 0x800EA5E8.
 * $a2 and $a3 pass through unchanged. GUESS: $a1 at the final jal 800E1CF0 is the last value set (see 800F59A0). */
uint32_t man_PL02_800DF52C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0, s2, q, s3, v0, v1, arg1;

    (void)a1;                                       /* $a1 is set (= 11) before the first jal */
    s2 = RD32(p + 0x130u);                          /* lw s2,0x130(s1) */
    q = RD32(p + 0x170u);                           /* lw s0,0x170(s1) */
    WR8(p + 0x07u, 0u);                             /* sb zero,0x7(s1) (jal delay slot, before the callee) */
    (void)m->call4(m->ctx, 0x800BB2A4u, p, 11u, a2, a3);   /* jal 800bb2a4; addiu a1,zero,11 */

    if (RD8(p + 0x207u) == 0u || RD8(s2 + 0x20u) != 0u) {  /* beq v0,zero (800df57c) / beq v0,zero (800df594) */
        WR8(p + 0x08u, 0u);                         /* sb zero,0x8(s1) */
        arg1 = RD32(0x800EA5D8u);                   /* lui v0,0x800f; lw a1,-0x5a28(v0) */
        return m->call4(m->ctx, 0x800268BCu, p, arg1, a2, a3);   /* jal 800268bc; then j 800df648 returns $v0 */
    }

    /* 800df594: main path */
    s2 = 1u;                                        /* addiu s2,zero,1 */
    s3 = 0x800EA5B8u;                               /* lui v0,0x800f; addiu s3,v0,-23112 */
    WR8(p + 0x08u, s2);                             /* sb s2,0x8(s1) */
    arg1 = RD32(s3 + 0x2Cu);                        /* lw a1,0x2c(s3) */
    (void)m->call4(m->ctx, 0x800268BCu, p, arg1, a2, a3);   /* jal 800268bc */

    WR8(q + 0x206u, 0u);                            /* sb zero,0x206(s0) */
    WR8(q + 0x207u, 2u);                            /* addiu v0,zero,2; sb v0,0x207(s0) */
    v1 = RD8(p + 0x207u);                           /* lbu v1,0x207(s1) */
    if (v1 == 2u) {                                 /* bne v1,v0 not taken (v0 = 2) */
        WR8(q + 0x207u, 3u);                        /* addiu v0,zero,3; sb v0,0x207(s0) */
        arg1 = RD32(s3 + 0x30u);                    /* lw a1,0x30(s3) */
        (void)m->call4(m->ctx, 0x800268BCu, p, arg1, a2, a3);  /* jal 800268bc; the delay addiu v0,zero,9 follows */
    }
    v0 = 9u;                                        /* 800df5e0: addiu v0,zero,9 (also the bne delay slot) */

    WR8(q + 0x00u, s2);                             /* sb s2,0x0(s0) */
    WR8(q + 0x01u, s2);                             /* sb s2,0x1(s0) */
    WR8(q + 0x1FAu, v0);                            /* sb v0,0x1fa(s0) */
    WR8(q + 0x05u, s2);                             /* sb s2,0x5(s0) */
    WR8(q + 0x06u, 0u);                             /* sb zero,0x6(s0) */
    WR8(q + 0x08u, 0u);                             /* sb zero,0x8(s0) */
    WR8(q + 0x07u, 0u);                             /* sb zero,0x7(s0) */
    WR16(q + 0x6Au, RD16(p + 0x6Au));               /* lhu v0,0x6a(s1); sh v0,0x6a(s0) */
    WR16(q + 0x6Eu, RD16(p + 0x6Eu));               /* lhu v0,0x6e(s1); sh v0,0x6e(s0) */
    WR8(p + 0x205u, s2);                            /* sb s2,0x205(s1) */
    WR8(q + 0x205u, s2);                            /* sb s2,0x205(s0) */
    WR8(p + 0x1FBu, 0u);                            /* sb zero,0x1fb(s1) */
    WR8(q + 0x1FBu, 0u);                            /* sb zero,0x1fb(s0) */
    WR8(q + 0x1F6u, RD8(p + 0x1F6u));               /* lbu v0,0x1f6(s1); sb v0,0x1f6(s0) */
    v0 = RD8(p + 0x0Bu);                            /* lbu v0,0xb(s1) */
    WR8(q + 0x0Bu, v0);                             /* addu a0,s0,zero; sb v0,0xb(a0) */
    WR8(p + 0x210u, s2);                            /* jal 800e1cf0 delay slot: sb s2,0x210(s1) */
    return m->call4(m->ctx, 0x800E1CF0u, q, arg1, a2, a3);   /* jal 800e1cf0 (a0 = Q) */
}
