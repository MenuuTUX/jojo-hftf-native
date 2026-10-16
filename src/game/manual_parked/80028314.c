#include "manual.h"
#include "hex2c_rt.h"

/* 80028314 (TEXT), frame 0x18 (addiu sp,sp,-24). $a1 is overwritten at entry (lui a1,0x8006 sets $a1 = 0x80060000), so the
 * incoming $a1 is not read. $a0 is loaded from 0x8005A930 (the record pointer p).
 *  - if byte p+0xC == 2: p+1 += 1, p+3 = 0, p+2 = 0, p+0xD = 32 (sb in the delay slot of the jal), then
 *    jal 80028E08 with $a0 = 0 (addu a0,zero,zero, before the jal), $a1 = 0x80060000, $a2 and $a3 unchanged (no write
 *    in this function). Returns the $v0 of 80028E08.
 *  - else returns 2 (the value in $v0 at the bne).
 * No callee runs before the jal, so every argument at the jal is exact. */
uint32_t man_80028314(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx), p, result = 2u;
    (void)a0;
    (void)a1;

    m->sp_set(m->ctx, sp - 24u);
    p = RD32(0x8005A930u);
    if (RD8(p + 0xCu) == 2u) {
        WR8(p + 1u, RD8(p + 1u) + 1u);
        WR8(p + 3u, 0u);
        WR8(p + 2u, 0u);
        WR8(p + 0xDu, 32u);
        result = m->call4(m->ctx, 0x80028E08u, 0u, 0x80060000u, a2, a3);
    }
    m->sp_set(m->ctx, sp);
    return result;
}
