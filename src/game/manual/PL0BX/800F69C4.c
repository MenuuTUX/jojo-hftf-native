#include "manual.h"
#include "hex2c_rt.h"

/* PL0BX 800F69C4..800F6A68 (164 bytes, slot B base 800F4800). Byte at a0+7 == 0: store byte+1, call 800268BC(a0,
 * RD32(0x800FE5C0)) and return its $v0. Byte != 0: jal 80026A04(a0); jal 800B46A4(a0); if that returns nonzero the function
 * returns 8 (the delay slot sets $v0 = 8). Otherwise it returns 800AF7EC(a0) when RD8(s1+0x1FA) != 8 or RD8(a0+0x205) == 0,
 * returns -1 when RDS8(a0+0x4D) != -1, and otherwise returns 800AF7EC(a0).
 * The generated C read RD32(0x80100C94); the lui v0,0x8010 / lw a1,-0x1a40(v0) pair addresses 0x800FE5C0.
 * $a2 and $a3 pass through; $a1 passes through to the 80026A04 / 800B46A4 / 800AF7EC calls as the caller left it. */
uint32_t man_PL0BX_800F69C4(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t old, s1, arg1, r;

    old = RD8(a0 + 7u);                             /* lbu v0,0x7(s0) */
    s1 = RD32(a0 + 0x170u);                         /* lw s1,0x170(s0) */
    if (old == 0u) {                                /* bne v0,zero,800f6a04 not taken */
        WR8(a0 + 7u, old + 1u);                     /* sb v0,0x7(s0) */
        arg1 = RD32(0x800FE5C0u);                   /* lui v0,0x8010; lw a1,-0x1a40(v0) */
        return m->call4(m->ctx, 0x800268BCu, a0, arg1, a2, a3);    /* jal 800268bc; the j 800f6a54 returns its $v0 */
    }
    (void)m->call4(m->ctx, 0x80026A04u, a0, a1, a2, a3);           /* jal 80026a04 */
    r = m->call4(m->ctx, 0x800B46A4u, a0, a1, a2, a3);             /* jal 800b46a4 */
    if (r != 0u) {                                  /* bne v0,zero,800f6a54; delay slot addiu v0,zero,8 */
        return 8u;
    }
    if (RD8(s1 + 0x1FAu) != 8u) {                   /* lbu v1,0x1fa(s1); bne v1,v0 (v0 = 8) */
        return m->call4(m->ctx, 0x800AF7ECu, a0, a1, a2, a3);      /* 800f6a4c */
    }
    if (RD8(a0 + 0x205u) == 0u) {                   /* lbu v0,0x205(s0); beq v0,zero,800f6a4c */
        return m->call4(m->ctx, 0x800AF7ECu, a0, a1, a2, a3);
    }
    if (RDS8(a0 + 0x4Du) != -1) {                   /* lb v1,0x4d(s0); bne v1,v0 (v0 = -1) */
        return 0xFFFFFFFFu;                         /* addiu v0,zero,-1 */
    }
    return m->call4(m->ctx, 0x800AF7ECu, a0, a1, a2, a3);          /* 800f6a4c */
}
