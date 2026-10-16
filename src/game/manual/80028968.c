#include "manual.h"
#include "hex2c_rt.h"

/* 80028968 (TEXT), frame 0x28 (addiu sp,sp,-40). The generated fn_80028968 passes the incoming $a2/$a3 to 80028E08, but the
 * original overwrites both: lwl/lwr at 800289A8..B0 (base 0x8006CEB8) or 800289E0..E8 (base 0x8006CEC8, chosen by bit 0 of
 * 0x80066C67) load $a2 = word at base and $a3 = word at base+4. The halfword at sp+0x16 is base+6. $a1 is overwritten at
 * 80028A24 (lui a1,0x8006) only on the 0x840 path, after the jal 8002438C. $a0 is never read as an input.
 *
 * Paths (p = word at 0x8005A930, record bytes p+2, p+8, p+0xC):
 *  - p+2 != 0: returns that byte.
 *  - (w & 0x840) != 0: jal 8002438C(138); then $a1 = 0x80060000; if p+8 != 0: jal 8002917C, then jal 80028E08(0, ...);
 *    else 5/other branch (no 80028E08; returns the $v0 of 800173E4 or 80029434).
 *  - (w & 0x10) == 0 and (w & 0x1000): p+8 = (p+8 ? p+8-1 : 1); jal 8002438C(137); jal 80028E08(1, ...).
 *  - (w & 0x10) == 0, 0x1000 clear, (w & 0x4000) clear: jal 80028E08(1, entry $a1, loaded $a2, loaded $a3), no callee first.
 *  - (w & 0x4000) set: p+8 = (p+8 == 1 ? 0 : p+8+1); jal 8002438C(137); jal 80028E08(1, ...).
 *  - (w & 0x10) set (0x840 clear): jal 8002917C, then jal 80028E08(0, ...).
 * Register values at the 80028E08 jal (the $a1..$a3 that the original passes):
 *  - no callee before the jal: $a1 = entry $a1 (or 0x80060000 on the 0x840 path), $a2/$a3 = the loaded words.
 *  - after 8002438C's own code: $a1 = 0, $a2 = 137 or 138, $a3 = 192. Then 80023AF4 runs: NOT MEASURED.
 *  - after 8002917C -> 800173E4's own code: $a1 = byte at 0x8006CE58, $a2 = 0x80080000, $a3 unchanged.
 *    Then 8004CB00 -> 8004C248: NOT MEASURED ($a3 could change there).
 * 80028E08 never reads $a2/$a3, and 80028D9C rewrites $a1 before 80028E08 reads it, so these leftovers do not reach its
 * result (measured from the disassembly of 80028E08 and 80028D9C; 800121F0 writes $a2/$a3 before reading them on the paths read). */
uint32_t man_80028968(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx) - 0x28u, p, x, w, v, ret = 0u;
    (void)a0;

    m->sp_set(m->ctx, sp);
    p = RD32(0x8005A930u);
    if (RD8(p + 2u) != 0u) {
        ret = RD8(p + 2u);
        goto done;
    }
    x = (RD8(0x80066C67u) & 1u) != 0u ? 0x8006CEB8u : 0x8006CEC8u;
    a2 = RD32(x);
    a3 = RD32(x + 4u);
    w = RD16(x + 6u);

    if ((w & 0x840u) != 0u) {
        m->call4(m->ctx, 0x8002438Cu, 138u, a1, a2, a3);
        a1 = 0u;
        a2 = 138u;
        a3 = 192u;
        a1 = 0x80060000u;                      /* lui a1,0x8006 at 80028A24, after the jal */
        p = RD32(0x8005A930u);
        if (RD8(p + 8u) != 0u) {
            m->call4(m->ctx, 0x8002917Cu, 0u, 0u, 0u, 0u);
            a1 = RD8(0x8006CE58u);             /* 800173E4 copies its $a0 into $a1 */
            a2 = 0x80080000u;                  /* lui a2,0x8008 in 800173E4 */
            ret = m->call4(m->ctx, 0x80028E08u, 0u, a1, a2, a3);
        } else if (RDS8(0x80066C6Cu) == 5) {
            WR8(p + 2u, 1u);
            WR8(p + 0xCu, 0u);
            WR16(0x80066C4Au, 2u);
            ret = m->call4(m->ctx, 0x800173E4u, RD8(0x8006CE58u), 0u, 0u, 0u);
        } else {
            WR8(p + 0xCu, 0u);
            m->call4(m->ctx, 0x800173E4u, RD8(0x8006CE58u), 0u, 0u, 0u);
            ret = m->call4(m->ctx, 0x80029434u, 0u, 0u, 0u, 0u);
        }
    } else if ((w & 0x10u) == 0u) {
        if ((w & 0x1000u) != 0u) {
            v = RD8(p + 8u) != 0u ? RD8(p + 8u) - 1u : 1u;
            WR8(p + 8u, v);
            m->call4(m->ctx, 0x8002438Cu, 137u, a1, a2, a3);
            a1 = 0u;
            a2 = 137u;
            a3 = 192u;
            ret = m->call4(m->ctx, 0x80028E08u, 1u, a1, a2, a3);
        } else if ((w & 0x4000u) == 0u) {
            ret = m->call4(m->ctx, 0x80028E08u, 1u, a1, a2, a3);
        } else {
            v = RD8(p + 8u);
            WR8(p + 8u, v == 1u ? 0u : v + 1u);
            m->call4(m->ctx, 0x8002438Cu, 137u, a1, a2, a3);
            a1 = 0u;
            a2 = 137u;
            a3 = 192u;
            ret = m->call4(m->ctx, 0x80028E08u, 1u, a1, a2, a3);
        }
    } else {
        m->call4(m->ctx, 0x8002917Cu, 0u, 0u, 0u, 0u);
        a1 = RD8(0x8006CE58u);
        a2 = 0x80080000u;
        ret = m->call4(m->ctx, 0x80028E08u, 0u, a1, a2, a3);
    }

done:
    m->sp_set(m->ctx, sp + 0x28u);
    return ret;
}
