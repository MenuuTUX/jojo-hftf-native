#include "manual.h"
#include "hex2c_rt.h"

/* PL0BX 800F59A0..800F5A2C (140 bytes, slot B base 800F4800). Same front half as 800F591C. Byte at a0+7 != 0: jal 80026A04(a0),
 * then 800AF7EC(a0) unless RD8(s1+0x1FA) == 8 and RD8(s1+6) == 0. Byte == 0: store byte+1 at a0+7 and call
 * 800268BC(a0, RD32(0x800FE5C0), ...). Both paths end with a tail jump into jal 800BB7BC(a0) whose $v0 is returned.
 * The generated C read RD32(0x80100C94); the lui v0,0x8010 / lw a1,-0x1a40(v0) pair addresses 0x800FE5C0.
 * $a1 is passed through to the first two calls as the caller left it; $a2 and $a3 are passed unchanged everywhere. */
uint32_t man_PL0BX_800F59A0(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t old, s1, arg1 = a1;

    old = RD8(a0 + 7u);                             /* lbu v0,0x7(s0) */
    s1 = RD32(a0 + 0x170u);                         /* lw s1,0x170(s0) */
    if (old != 0u) {                                /* bne v0,zero,800f59e0 */
        (void)m->call4(m->ctx, 0x80026A04u, a0, a1, a2, a3);       /* jal 80026a04 */
        if (RD8(s1 + 0x1FAu) != 8u || RD8(s1 + 6u) != 0u) {        /* bne v1,v0 / beq v0,zero (exit to 800f5a10) */
            (void)m->call4(m->ctx, 0x800AF7ECu, a0, a1, a2, a3);   /* jal 800af7ec */
        }
    } else {
        WR8(a0 + 7u, old + 1u);                     /* sb v0,0x7(s0) */
        arg1 = RD32(0x800FE5C0u);                   /* lui v0,0x8010; lw a1,-0x1a40(v0) */
        (void)m->call4(m->ctx, 0x800268BCu, a0, arg1, a2, a3);     /* jal 800268bc; its $v0 is discarded */
    }
    /* 800f5a10: jal 800bb7bc, delay slot addu a0,s0,zero. GUESS: $a1 here is taken as the last value this function set
     * (the caller's a1 on the byte != 0 path, the loaded value on the byte 0 path). If a callee overwrites $a1, the MIPS
     * passes that value instead; this is not modelled. */
    return m->call4(m->ctx, 0x800BB7BCu, a0, arg1, a2, a3);
}
