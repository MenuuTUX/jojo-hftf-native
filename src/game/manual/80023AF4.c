#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_80023C48(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);

/* 80023AF4 (TEXT), frame 0x38 (addiu sp,sp,-56). $a0 = index: 128 clears four byte/halfword tables at 0x8008EBA8..0x8008EBB7
 * (after 8003F940(0, $a1, $a2, $a3)) and returns 0x80089FFF. Any other $a0: the byte at 0x8008A000+$a0 (signed) indexes the
 * pointer table at 0x8005A7D0, the entry it selects plus 8*($a2 & 0xFFFF) is walked, and 80023C48 is called for each record
 * while bit 23 of the record's second word is set; it returns 0. The 5th argument (signed byte) is at the caller's sp+0x10
 * (our sp+0x48); the outgoing stack words sp+0x10 (signed $a3 halfword) and sp+0x14 (the 5th argument) are for 80023C48. */
uint32_t man_80023AF4(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx) - 0x38u;   /* 0x38 = the original "addiu sp,sp,-56" */
    const uint32_t tab = 0x8008A000u;          /* $s0 in the branch of 128: lui 0x8009, addiu -0x6000 */
    uint32_t s0, s3, s4, v17, v, ret;
    int32_t i;

    m->sp_set(m->ctx, sp);
    if (a0 == 0x80u) {
        m->call4(m->ctx, 0x8003F940u, 0u, a1, a2, a3);
        for (i = 3; i >= 0; i--) WR8(tab + 0x4BA8u + (uint32_t)i, 0u);
        for (i = 3; i >= 0; i--) WR16(tab + 0x4B9Cu + 2u * (uint32_t)i, 0u);
        for (i = 3; i >= 0; i--) WR8(tab + 0x4BA4u + (uint32_t)i, 0u);
        for (i = 3; i >= 0; i--) WR8(tab + 0x4BB4u + (uint32_t)i, 0u);
        ret = tab - 1u;                        /* $v0 left by the last loop */
    } else {
        s4 = (uint32_t)(int32_t)(int16_t)(a3 & 0xFFFFu);
        s3 = (uint32_t)(int32_t)(int8_t)(RD32(sp + 0x48u) & 0xFFu);
        v17 = (uint32_t)(int32_t)RDS8(tab + a0);
        WR16(0x80089FF8u, (uint16_t)v17);
        s0 = RD32(0x8005A7D0u + v17 * 4u) + ((a2 & 0xFFFFu) << 3);
        do {
            WR32(sp + 0x10u, s4);
            WR32(sp + 0x14u, s3);
            man_80023C48(m, s0, a0, a1, a2 & 0xFFFFu);
            v = RD32(s0 + 4u);
            s0 += 8u;
        } while (((v >> 23) & 1u) != 0u);
        ret = 0u;                              /* $v0 after the loop: the loop only exits when the bit is 0 */
    }
    m->sp_set(m->ctx, sp + 0x38u);
    return ret;
}
