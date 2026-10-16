#include "manual.h"
#include "hex2c_rt.h"

/* VS_SEL 800E0798..800E084C. $a0 = P. Sets a block of fields of P (constants), clears P+1 and the halfwords at P+0xD0..0xD8,
 * sets P+0xDC/0xDE to 16, then P+0x6A = (byte P+4 nonzero ? 280 : 16) and P+0x6E = 87, stores byte P+5 + 1, then jal 800258f0
 * with a0 = P, a1 = 1, and a2/a3 as received; returns its $v0. */
uint32_t man_VS_SEL_800E0798(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;
    uint32_t sp = m->sp_get(m->ctx) - 24u;               /* addiu sp,sp,-24 */
    uint32_t v, w, r;
    m->sp_set(m->ctx, sp);

    WR8(p + 0x32u, 13u);                                  /* sb v0,0x32(a0), v0 = 13 */
    WR8(p + 0xB0u, 162u);                                 /* sb v0,0xb0(a0) */
    WR16(p + 0xF0u, 63u);                                 /* sh v0,0xf0(a0), v0 = 63 */
    WR16(p + 0xEEu, 63u);                                 /* sh v0,0xee(a0) */
    WR16(p + 0xD6u, 88u);                                 /* sh v0,0xd6(a0), v0 = 88 */
    WR16(p + 0xD2u, 88u);                                 /* sh v0,0xd2(a0) */
    WR8(p + 0xE0u, 48u);                                  /* sb v0,0xe0(a0), v0 = 48 */
    WR8(p + 0xE4u, 48u);                                  /* sb v0,0xe4(a0) */
    WR8(p + 0xE8u, 128u);                                 /* sb v0,0xe8(a0), v0 = 128 */
    WR8(p + 0xB8u, 128u);                                 /* sb v0,0xb8(a0) */
    v = RD8(p + 5u);                                      /* lbu v0,5(a0) */
    w = RD8(p + 4u);                                      /* lbu v1,4(a0) */
    WR8(p + 1u, 0u);                                      /* sb zero,1(a0) */
    WR16(p + 0xF2u, 0u);                                  /* sh zero,0xf2(a0) */
    WR16(p + 0xF4u, 0u);                                  /* sh zero,0xf4(a0) */
    WR16(p + 0xD4u, 0u);                                  /* sh zero,0xd4(a0) */
    WR16(p + 0xD0u, 0u);                                  /* sh zero,0xd0(a0) */
    WR16(p + 0xDAu, 0u);                                  /* sh zero,0xda(a0) */
    WR16(p + 0xD8u, 0u);                                  /* sh zero,0xd8(a0) */
    WR16(p + 0xDEu, 16u);                                 /* sh a1,0xde(a0), a1 = 16 */
    WR16(p + 0xDCu, 16u);                                 /* sh a1,0xdc(a0) */
    WR8(p + 5u, v + 1u);                                  /* addiu v0,v0,1; sb v0,5(a0) */
    WR16(p + 0x6Au, w != 0u ? 280u : 16u);                /* 800e0828 (sh v0 = 280) / delay slot sh a1 = 16 */
    WR16(p + 0x6Eu, 87u);                                 /* sh v0,0x6e(a0), v0 = 87 */
    r = m->call4(m->ctx, 0x800258F0u, p, 1u, a2, a3);    /* jal 800258f0, delay slot addiu a1,zero,1 */

    m->sp_set(m->ctx, sp + 24u);
    return r;
}
