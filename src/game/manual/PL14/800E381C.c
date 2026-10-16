#include "manual.h"
#include "hex2c_rt.h"

/* PL14 800E381C..800E38B0 (slot A, base 800DF000). $a0 = P (kept in $s0). $a1 is not written before the first jal, so it
 * is the entry value passed on (nargs 4). The first jal 800bc8a8 gets $a0 = P, $a1 = entry $a1, and $a2/$a3 entry values;
 * its delay slot `sh v1,0x1e(s0)` stores the halfword read from +0x84 before the call. The second jal 800268bc gets
 * $a0 = P, $a1 = word 0x800E8C3C, and $a2/$a3 as the first callee left them. The return is $v0 of 800268bc. */
uint32_t man_PL14_800E381C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;
    uint32_t v;
    uint16_t h = RD16(p + 0x84u);                       /* lhu v1,0x84(s0) (800e382c) */
    WR8(p + 0x1FAu, 4u);
    WR32(p + 0x74u, 0xFFFD0000u);                       /* lui v0,0xfffd; sw v0,0x74(s0) */
    WR16(p + 0x18Eu, 1u);
    WR32(p + 0x7Cu, 0u);
    WR32(p + 0x78u, 0u);
    WR32(p + 0x70u, 0u);
    WR8(p + 6u, 0u);
    WR8(p + 7u, 2u);
    WR8(p + 9u, 0u);
    WR8(p + 8u, 0u);
    WR16(p + 0x1E6u, 0u);
    WR16(p + 0x1E4u, 0u);
    WR8(p + 0x200u, 0u);
    WR8(p + 0x20Bu, 0u);
    WR8(p + 0x209u, 0u);
    WR8(p + 0x1FFu, 0u);
    WR8(p + 0x227u, 0u);
    WR8(p + 0x14Eu, 0u);
    WR8(p + 0x163u, 0u);
    WR16(p + 0x1Eu, h);                                 /* sh v1,0x1e(s0): delay slot of jal 800bc8a8 */
    m->call4(m->ctx, 0x800BC8A8u, p, a1, a2, a3);
    a1 = m->gpr_get(m->ctx, 5u);                        /* callee-left $a1..$a3 (not used below except $a2/$a3) */
    a2 = m->gpr_get(m->ctx, 6u);
    a3 = m->gpr_get(m->ctx, 7u);
    v = RD32(0x800E8C3Cu);                              /* lui v0,0x800f; lw a1,-0x73c4(v0) (800e3898) */
    return m->call4(m->ctx, 0x800268BCu, p, v, a2, a3); /* jal 800268bc, delay addu a0,s0,zero */
}
