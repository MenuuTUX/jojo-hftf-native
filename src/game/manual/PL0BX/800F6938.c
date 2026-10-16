#include "manual.h"
#include "hex2c_rt.h"

/* PL0BX 800F6938..800F69C4 (140 bytes, slot B base 800F4800). Byte at a0+7 != 0: jal 80026A04(a0); then 800AF7EC(a0) when
 * RD8(s1+0x1FA) != 8 or RD8(a0+0x205) == 0 (the call falls through into the tail). Byte == 0: store byte+1 at a0+7 and
 * call 800268BC(a0, RD32(0x800FE5C0)). Both paths end with jal 800BB7BC(a0), whose $v0 is returned.
 * The generated C read RD32(0x80100C94) and passed 0 for $a1..$a3; the lui v0,0x8010 / lw a1,-0x1a40(v0) pair addresses
 * 0x800FE5C0. $a2 and $a3 pass through unchanged. GUESS: $a1 at the final jal is the last value set (see 800F59A0). */
uint32_t man_PL0BX_800F6938(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t old, s1, arg1 = a1;

    old = RD8(a0 + 7u);                             /* lbu v0,0x7(s0) */
    s1 = RD32(a0 + 0x170u);                         /* lw s1,0x170(s0) */
    if (old != 0u) {                                /* bne v0,zero,800f6978 */
        (void)m->call4(m->ctx, 0x80026A04u, a0, a1, a2, a3);       /* jal 80026a04 */
        if (RD8(s1 + 0x1FAu) != 8u || RD8(a0 + 0x205u) == 0u) {    /* bne v1,v0 / bne v0,zero (skip to 800f69a8) */
            (void)m->call4(m->ctx, 0x800AF7ECu, a0, a1, a2, a3);   /* jal 800af7ec (falls into the tail) */
        }
    } else {
        WR8(a0 + 7u, old + 1u);                     /* sb v0,0x7(s0) */
        arg1 = RD32(0x800FE5C0u);                   /* lui v0,0x8010; lw a1,-0x1a40(v0) */
        (void)m->call4(m->ctx, 0x800268BCu, a0, arg1, a2, a3);     /* jal 800268bc; $v0 discarded */
    }
    return m->call4(m->ctx, 0x800BB7BCu, a0, arg1, a2, a3);        /* 800f69a8: jal 800bb7bc */
}
