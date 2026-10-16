#include "manual.h"
#include "hex2c_rt.h"

/* PL07X 800F4A90..800F4AC8 (56 bytes, slot B base 800F4800). jal 800BB2A4 with $a0 = a0 (unchanged) and $a1 = 0 (delay
 * slot), then jal 800268BC with $a0 = a0, $a1 = RD32(0x800FB1C0), $a2 and $a3 unchanged. Returns the second callee's $v0.
 * The generated C read RD32(0x800FBA2C); the lui v0,0x8010 / lw a1,-0x4e40(v0) pair addresses 0x800FB1C0.
 * The first call's $a2 and $a3 are the caller's values, passed on unchanged. */
uint32_t man_PL07X_800F4A90(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t v0, arg1;

    (void)a1;                                       /* $a1 is written (= 0) before the first jal */
    (void)m->call4(m->ctx, 0x800BB2A4u, a0, 0u, a2, a3);    /* jal 800bb2a4; delay slot addu a1,zero,zero */
    arg1 = RD32(0x800FB1C0u);                       /* lui v0,0x8010; lw a1,-0x4e40(v0) */
    v0 = m->call4(m->ctx, 0x800268BCu, a0, arg1, a2, a3);   /* jal 800268bc; delay slot addu a0,s0,zero (= a0) */
    return v0;
}
