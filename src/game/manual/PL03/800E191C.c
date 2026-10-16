#include "manual.h"
#include "hex2c_rt.h"

/* PL03 800E191C..800E19D4 (184 bytes, slot A base 800DF000). Byte at a0+8 == 0: store byte+1 there. Then if RD8(a0+0x206)
 * == 22, jal 800268BC with RD32(0x800EA930), else with RD32(0x800EA8BC); both pass $a2, $a3 unchanged. Byte != 0: if
 * RD8(a0+0x206) == 22 call 80026A04(a0), else 800E2BB0(a0); then return 8 if RD8(s1+0x1FA) == 8 (s1 = RD32(a0+0x170)),
 * otherwise return 800AF7EC(a0). The generated C read RD32(0x800EC020) and RD32(0x800EB9FC); the lui v0,0x800f with
 * lw a1,-0x5744(v0) and lw a1,-0x56d0(v0) address 0x800EA8BC and 0x800EA930. $a1, $a2, $a3 pass through unchanged on the
 * calls where the MIPS does not set them. */
uint32_t man_PL03_800E191C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t old, s1, v1, arg1;

    old = RD8(a0 + 8u);                             /* lbu v0,0x8(s0) */
    s1 = RD32(a0 + 0x170u);                         /* lw s1,0x170(s0) */
    if (old == 0u) {                                /* bne v0,zero,800e1980 not taken */
        v1 = RD8(a0 + 0x206u);                      /* lbu v1,0x206(s0) */
        WR8(a0 + 8u, old + 1u);                     /* sb v0,0x8(s0) (v0 = byte + 1) */
        if (v1 == 22u) {                            /* beq v1,v0 (v0 = 22) -> 800e1968 */
            arg1 = RD32(0x800EA930u);               /* lui v0,0x800f; lw a1,-0x56d0(v0) */
        } else {
            arg1 = RD32(0x800EA8BCu);               /* lui v0,0x800f; lw a1,-0x5744(v0) */
        }
        return m->call4(m->ctx, 0x800268BCu, a0, arg1, a2, a3);   /* jal 800268bc; delay nop */
    }
    v1 = RD8(a0 + 0x206u);                          /* 800e1980: lbu v1,0x206(s0) */
    if (v1 == 22u) {
        (void)m->call4(m->ctx, 0x80026A04u, a0, a1, a2, a3);      /* jal 80026a04 */
    } else {
        (void)m->call4(m->ctx, 0x800E2BB0u, a0, a1, a2, a3);      /* jal 800e2bb0 */
    }
    if (RD8(s1 + 0x1FAu) == 8u) {                   /* 800e19a8: lbu v1,0x1fa(s1); beq v1,v0 (v0 = 8) */
        return 8u;                                  /* addiu v0,zero,8 */
    }
    return m->call4(m->ctx, 0x800AF7ECu, a0, a1, a2, a3);         /* jal 800af7ec */
}
