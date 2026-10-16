#include "manual.h"
#include "hex2c_rt.h"

/* VS_SEL 800E06F8..800E0798. $a0 = P. Sets fields of P, stores byte(P+5)+1, reads the signed byte at (word P+0x80)+8, stores
 * the 32-bit pointer 0x800E24F4 at P+0xAC, sets halfword P+0x10 = that byte + 2, halfword P+0x6A = (P+4 nonzero ? 328 : 64),
 * P+0x6E = 89, then jal 800258f0 with a0 = P, a1 = 1, and a2/a3 as received; returns its $v0. */
uint32_t man_VS_SEL_800E06F8(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;
    uint32_t sp = m->sp_get(m->ctx) - 24u;               /* addiu sp,sp,-24 */
    uint32_t v, q, t, w, r;
    m->sp_set(m->ctx, sp);

    WR8(p + 1u, 1u);                                      /* sb v0,1(a0), v0 = 1 */
    WR8(p + 0xB0u, 3u);                                   /* sb v0,0xb0(a0), v0 = 3 */
    WR8(p + 0x32u, 13u);                                  /* sb v0,0x32(a0), v0 = 13 */
    v = RD8(p + 5u);                                      /* lbu v0,5(a0) */
    q = RD32(p + 0x80u);                                  /* lw v1,0x80(a0) */
    WR8(p + 0xB1u, 0u);                                   /* sb zero,0xb1(a0) */
    WR8(p + 0xB8u, 0u);                                   /* sb zero,0xb8(a0) */
    WR8(p + 5u, v + 1u);                                  /* addiu v0,v0,1; sb v0,5(a0) */
    t = RD8(q + 8u);                                      /* lbu v1,8(v1) */
    WR16(p + 0x1Eu, 1u);                                  /* sh v0,0x1e(a0), v0 = 1 */
    WR8(p + 0x1Du, 0xFFu);                                /* sb v0,0x1d(a0), v0 = -1 */
    WR32(p + 0xACu, 0x800E24F4u);                         /* lui/addiu, sw v0,0xac(a0) */
    w = RD8(p + 4u);                                      /* lbu v0,4(a0) */
    WR16(p + 0x10u, (uint32_t)((int32_t)(int8_t)t + 2)); /* sll/sra 24, addiu v1,v1,2; sh v1,0x10(a0) (delay slot) */
    WR16(p + 0x6Au, w != 0u ? 328u : 64u);                /* 800e0770 / 800e0768 */
    WR16(p + 0x6Eu, 89u);                                 /* sh v0,0x6e(a0), v0 = 89 */
    r = m->call4(m->ctx, 0x800258F0u, p, 1u, a2, a3);    /* jal 800258f0, delay slot addiu a1,zero,1 */

    m->sp_set(m->ctx, sp + 24u);
    return r;
}
