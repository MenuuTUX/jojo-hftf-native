#include "manual.h"
#include "hex2c_rt.h"

/* PL0BX 800F591C..800F5998 (132 bytes, slot B base 800F4800). Reads the byte at a0+7 and the pointer s1 = RD32(a0+0x170).
 * Byte != 0: jal 80026A04(a0), then if RD8(s1+0x1FA) != 8 or RD8(s1+6) != 0 it returns 800AF7EC(a0) (the byte-6 zero exit
 * returns 0). Byte == 0: store byte+1 at a0+7 and return 800268BC(a0, RD32(0x800FE5C0), ...).
 * The generated C read RD32(0x80100C94); the lui v0,0x8010 / lw a1,-0x1a40(v0) pair addresses 0x800FE5C0.
 * The calls to 80026A04, 800AF7EC and 800268BC pass $a1 through (the caller's value on the first two calls), and $a2 and
 * $a3 unchanged on all three. */
uint32_t man_PL0BX_800F591C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t old, s1, arg1;

    old = RD8(a0 + 7u);                             /* lbu v0,0x7(s0) */
    s1 = RD32(a0 + 0x170u);                         /* lw s1,0x170(s0) */
    if (old != 0u) {                                /* bne v0,zero,800f595c; delay slot addiu v0,v0,1 has no store */
        (void)m->call4(m->ctx, 0x80026A04u, a0, a1, a2, a3);       /* jal 80026a04 */
        if (RD8(s1 + 0x1FAu) != 8u) {               /* lbu v1,0x1fa(s1); addiu v0,zero,8; bne v1,v0 */
            return m->call4(m->ctx, 0x800AF7ECu, a0, a1, a2, a3);  /* jal 800af7ec */
        }
        if (RD8(s1 + 6u) == 0u) {                   /* lbu v0,0x6(s1); beq v0,zero,800f598c: v0 = 0 is returned */
            return 0u;
        }
        return m->call4(m->ctx, 0x800AF7ECu, a0, a1, a2, a3);      /* jal 800af7ec */
    }
    WR8(a0 + 7u, old + 1u);                         /* sb v0,0x7(s0) (v0 = byte + 1, from the delay slot) */
    arg1 = RD32(0x800FE5C0u);                       /* lui v0,0x8010; lw a1,-0x1a40(v0) */
    return m->call4(m->ctx, 0x800268BCu, a0, arg1, a2, a3);       /* jal 800268bc; delay slot nop */
}
