#include "manual.h"
#include "hex2c_rt.h"

/* VS_SEL 800E0670..800E06F8. $a0 = P. Sets fields of P, stores byte(P+5)+1 back, sets the two halfword timers from byte P+4
 * (nonzero: 50 and 320; zero: 30 and 64), then jal 800258F0 with $a0 = P, $a1 = 1, and $a2/$a3 as received; returns its $v0. */
uint32_t man_VS_SEL_800E0670(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;
    uint32_t sp = m->sp_get(m->ctx) - 24u;               /* addiu sp,sp,-24 */
    uint32_t v, w, r;
    m->sp_set(m->ctx, sp);

    WR8(p + 1u, 1u);                                      /* sb v0,1(a0), v0 = 1 */
    WR8(p + 0xB0u, 1u);                                   /* sb v0,0xb0(a0) */
    WR16(p + 0x1Eu, 1u);                                  /* sh v0,0x1e(a0) */
    WR8(p + 0x1Du, 0xFFu);                                /* sb v0,0x1d(a0), v0 = -1 */
    WR8(p + 0x32u, 12u);                                  /* sb v0,0x32(a0), v0 = 12 */
    v = RD8(p + 5u);                                      /* lbu v0,5(a0) */
    w = RD8(p + 4u);                                      /* lbu v1,4(a0) */
    WR8(p + 0xB1u, 0u);                                   /* sb zero,0xb1(a0) */
    WR8(p + 0xB8u, 0u);                                   /* sb zero,0xb8(a0) */
    WR8(p + 5u, v + 1u);                                  /* addiu v0,v0,1; sb v0,5(a0) (bne delay slot, both paths) */
    WR16(p + 0x10u, w != 0u ? 50u : 30u);                 /* 800e06c8 / 800e06b8 */
    WR16(p + 0x6Au, w != 0u ? 320u : 64u);                /* sh v0,0x6a(a0) after the two branches */
    WR16(p + 0x6Eu, 184u);                                /* sh v0,0x6e(a0) */
    r = m->call4(m->ctx, 0x800258F0u, p, 1u, a2, a3);    /* jal 800258f0, delay slot addiu a1,zero,1 */

    m->sp_set(m->ctx, sp + 24u);
    return r;
}
