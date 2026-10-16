#include "manual.h"
#include "hex2c_rt.h"

/* PL0AX 800F4B60..800F4B98 (56 bytes, slot B base 800F4800). jal 800BB2A4 with $a0 = a0 (unchanged) and $a1 = 17
 * (delay slot), then jal 800268BC with $a0 = a0, $a1 = RD32(0x800FFA30), $a2 and $a3 unchanged. Returns the second
 * callee's $v0. The generated C read RD32(0x80100240); the lui v0,0x8010 / lw a1,-0x5d0(v0) pair addresses 0x800FFA30. */
uint32_t man_PL0AX_800F4B60(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t v0, arg1;

    (void)a1;                                       /* $a1 is written (= 17) before the first jal */
    (void)m->call4(m->ctx, 0x800BB2A4u, a0, 17u, a2, a3);   /* jal 800bb2a4; delay slot addiu a1,zero,17 */
    arg1 = RD32(0x800FFA30u);                       /* lui v0,0x8010; lw a1,-0x5d0(v0) */
    v0 = m->call4(m->ctx, 0x800268BCu, a0, arg1, a2, a3);   /* jal 800268bc; delay slot addu a0,s0,zero (= a0) */
    return v0;
}
