#include "manual.h"
#include "hex2c_rt.h"

/* PL02 800E0680..800E07F8 (376 bytes, slot A base 800DF000). P = a0, Q = RD32(P+0x170), R = RD32(P+0x130).
 * If (int16)RD16(P+0x6E) >= 36: sets P+7 = 5; if RD32(P+0x7C) >= 0 also writes P+0x74 = 0xFFFF0000 and P+0x7C =
 * 0xFFFF8000; then returns 80026A04(P). Otherwise: if RD8(Q+0x205) == 0 return 800AF7EC(P). If RD8(P+8) == 0, call
 * 800268BC(P, RD32(0x800EA5A4)) and increment P+8. Then, when P+8 == 1 and RD8(R+0x20) != 0, decrement the halfword P+0xC and
 * (if it is now -1) call 800268BC(P, RD32(0x800EA5A8)) and increment P+8; when P+8 == 1 and RD8(R+0x20) == 0 and
 * RD8(Q+0x1FA) != 8, the same call and increment. When P+8 != 1 and RDS8(P+0x4D) == -1, call 800AF7EC(P). Finally
 * call 800BB708(P) unless RD8(P+0x1FB) == 2 or RD8(P+0x206) == 5, and return 80026A04(P)'s $v0.
 * The loads here are RD32(0x800EA5A4) = 0x800EACEC (first 800268BC argument) and RD32(0x800EA5A8) = 0x800EAD12 (second).
 * The generated C dereferenced the first value again (RD32(0x800EACEC)); the second call was already right.
 * $a2 and $a3 pass through unchanged. $a1 passes through until a 800268BC call loads it, then the loaded value (GUESS:
 * callee clobbers of $a1 not modelled). */
uint32_t man_PL02_800E0680(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0, q, r, arg1 = a1, v3;
    int32_t h;

    h = (int32_t)RDS16(p + 0x6Eu);                  /* lh v0,0x6e(s0) */
    q = RD32(p + 0x170u);                           /* lw s1,0x170(s0) */
    r = RD32(p + 0x130u);                           /* lw s2,0x130(s0) */

    if (h >= 36) {                                  /* slti v0,v0,36; bne v0,zero,800e06d4 not taken */
        WR8(p + 0x07u, 5u);                         /* addiu v0,zero,5; sb v0,0x7(s0) (bltz delay slot) */
        v3 = RD32(p + 0x7Cu);                       /* lw v3,0x7c(s0) */
        if ((int32_t)v3 >= 0) {                     /* bltz v3,800e07d8 not taken */
            WR32(p + 0x74u, 0xFFFF0000u);           /* lui v0,0xffff; sw v0,0x74(s0) */
            WR32(p + 0x7Cu, 0xFFFF8000u);           /* addiu v0,zero,-32768; j 800e07d8; sw v0,0x7c(s0) */
        }
        return m->call4(m->ctx, 0x80026A04u, p, arg1, a2, a3);     /* 800e07d8: jal 80026a04 */
    }

    /* 800e06d4 */
    if (RD8(q + 0x205u) == 0u) {                    /* lbu v0,0x205(s1); bne v0,zero,800e06f4 not taken */
        return m->call4(m->ctx, 0x800AF7ECu, p, arg1, a2, a3);    /* jal 800af7ec; j 800e07e0 */
    }

    /* 800e06f4 */
    if (RD8(p + 0x08u) == 0u) {                     /* lbu v0,0x8(s0); bne v0,zero,800e0720 not taken */
        arg1 = RD32(0x800EA5A4u);                   /* lui v0,0x800f; lw a1,-0x5a5c(v0) */
        (void)m->call4(m->ctx, 0x800268BCu, p, arg1, a2, a3);   /* jal 800268bc */
        WR8(p + 0x08u, RD8(p + 0x08u) + 1u);        /* lbu v0,0x8(s0); addiu v0,v0,1; sb v0,0x8(s0) */
    }

    /* 800e0720 */
    v3 = RD8(p + 0x08u);
    if (v3 == 1u) {                                 /* bne v1,v0 (v0 = 1) not taken */
        if (RD8(r + 0x20u) != 0u) {                 /* lbu v0,0x20(s2); beq v0,zero,800e0768 not taken */
            uint16_t hw = (uint16_t)(RD16(p + 0x0Cu) - 1u);      /* lhu v0,0xc(s0); addiu v0,v0,-1; sh v0,0xc(s0) */
            WR16(p + 0x0Cu, hw);
            if ((int16_t)hw == -1) {                /* sll/sra 16; bne v0,v1 (v1 = -1) not taken: 800e0760 */
                arg1 = RD32(0x800EA5A8u);           /* lui v0,0x800f; lw a1,-0x5a58(v0) (j 800e0778) */
                (void)m->call4(m->ctx, 0x800268BCu, p, arg1, a2, a3);   /* jal 800268bc */
                WR8(p + 0x08u, RD8(p + 0x08u) + 1u);            /* 800e0784: sb v0 = byte + 1 */
            }
        } else if (RD8(q + 0x1FAu) != 8u) {         /* lbu v3,0x1fa(s1); beq v3,v0 (v0 = 8) not taken: 800e0774 */
            arg1 = RD32(0x800EA5A8u);               /* lui v0,0x800f; lw a1,-0x5a58(v0) */
            (void)m->call4(m->ctx, 0x800268BCu, p, arg1, a2, a3);   /* jal 800268bc */
            WR8(p + 0x08u, RD8(p + 0x08u) + 1u);    /* 800e0784: sb v0 = byte + 1 */
        }
    } else {                                        /* 800e0798 */
        if (RDS8(p + 0x4Du) == -1) {                /* lb v3,0x4d(s0); bne v3,v0 (v0 = -1) not taken */
            (void)m->call4(m->ctx, 0x800AF7ECu, p, arg1, a2, a3);   /* jal 800af7ec */
        }
    }

    /* 800e07b0 */
    if (RD8(p + 0x1FBu) != 2u && RD8(p + 0x206u) != 5u) {   /* beq v3,v0 (2) and beq v1,v0 (5) skip the call */
        (void)m->call4(m->ctx, 0x800BB708u, p, arg1, a2, a3);   /* jal 800bb708 */
    }
    return m->call4(m->ctx, 0x80026A04u, p, arg1, a2, a3);     /* 800e07d8: jal 80026a04; the return is its $v0 */
}
