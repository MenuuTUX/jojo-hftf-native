#include "manual.h"
#include "hex2c_rt.h"

/* 800113F0: clears the object at a0 (two 0x40-byte-stride sub-objects) and the
 * global tables at 0x80073E98..0x800743D6, 0x80076DF0..0x80076E62, then calls
 * 800399F0 on a0 and on a0+12, returning the second call's $v0. The first call
 * to 800116F8 passes $a0..$a3 through unchanged (nargs = 4). */
uint32_t man_800113F0(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t s0 = a0;
    uint32_t s1 = 0x800743D0u;   /* lui 8007 + addiu 0x43D0 (delay slot of jal 800116F8) */
    uint32_t s2 = 0x80075C90u;   /* lui 8007 + addiu 0x5C90 */
    uint32_t s3 = 0x80076DF0u;   /* lui 8007 + addiu 0x6DF0 */
    uint32_t s4 = 0x80076E50u;   /* lui 8007 + addiu 0x6E50 */
    uint32_t p;
    uint32_t n;

    m->call4(m->ctx, 0x800116F8u, a0, a1, a2, a3);

    WR16(s0 + 0x24u, 0u);
    WR16(s0 + 0x22u, 0u);
    WR16(s0 + 0x20u, 0u);
    WR16(s0 + 0x26u, 0u);
    WR16(s0 + 0x1Eu, 0u);
    WR16(s0 + 0x1Cu, 0u);
    WR16(s0 + 0x18u, 0u);
    WR16(s0 + 0x1Au, 510u);

    /* 49 records at 0x80073E98, stride -8 */
    p = 0x80073E98u;
    n = 49u;
    do {
        WR16(p + 6u, 0u);
        WR16(p + 4u, 0u);
        WR8(p + 3u, 0u);
        WR8(p + 2u, 0u);
        WR8(p + 1u, 0u);
        WR8(p + 0u, 0u);
        n--;
        p -= 8u;
    } while ((int32_t)n > 0);

    /* 29 records at 0x80073F88, stride -8 */
    p = 0x80073F88u;
    n = 29u;
    do {
        WR16(p + 6u, 0u);
        WR16(p + 4u, 0u);
        WR8(p + 3u, 0u);
        WR8(p + 2u, 0u);
        WR8(p + 1u, 0u);
        WR8(p + 0u, 0u);
        n--;
        p -= 8u;
    } while ((int32_t)n > 0);

    /* 2 records at 0x80073FA0, stride -8 */
    p = 0x80073FA0u;
    n = 2u;
    do {
        WR16(p + 6u, 0u);
        WR16(p + 4u, 0u);
        WR8(p + 3u, 0u);
        WR8(p + 2u, 0u);
        WR8(p + 1u, 0u);
        WR8(p + 0u, 0u);
        n--;
        p -= 8u;
    } while ((int32_t)n > 0);

    /* 2 records at 0x80073FC0, stride -8 */
    p = 0x80073FC0u;
    n = 2u;
    do {
        WR16(p + 6u, 0u);
        WR16(p + 4u, 0u);
        WR8(p + 3u, 0u);
        WR8(p + 2u, 0u);
        WR8(p + 1u, 0u);
        WR8(p + 0u, 0u);
        n--;
        p -= 8u;
    } while ((int32_t)n > 0);

    /* 197 halfwords at 0x8007420A, stride -2 */
    p = 0x8007420Au;
    n = 197u;
    do {
        WR16(p, 0u);
        n--;
        p -= 2u;
    } while ((int32_t)n > 0);

    /* 110 halfwords at 0x800742EC, stride -2 */
    p = 0x800742ECu;
    n = 110u;
    do {
        WR16(p, 0u);
        n--;
        p -= 2u;
    } while ((int32_t)n > 0);

    /* 2 halfwords at 0x800742F4, stride -2 */
    p = 0x800742F4u;
    n = 2u;
    do {
        WR16(p, 0u);
        n--;
        p -= 2u;
    } while ((int32_t)n > 0);

    /* 102 halfwords at 0x800743CC, stride -2 */
    p = 0x800743CCu;
    n = 102u;
    do {
        WR16(p, 0u);
        n--;
        p -= 2u;
    } while ((int32_t)n > 0);

    /* 396 records at s1+6, stride +16 */
    p = s1 + 6u;
    n = 396u;
    do {
        WR8(p - 3u, 3u);
        WR8(p + 1u, 116u);
        WR8(p - 2u, 128u);
        WR8(p - 1u, 128u);
        WR8(p + 0u, 128u);
        n--;
        p += 16u;
    } while (n != 0u);

    /* 222 records at s2+0x12, stride +20 */
    p = s2 + 0x12u;
    n = 222u;
    do {
        WR8(p - 0xFu, 4u);
        WR8(p - 0xBu, 100u);
        WR8(p - 0xEu, 128u);
        WR8(p - 0xDu, 128u);
        WR8(p - 0xCu, 128u);
        WR16(p - 2u, 8u);
        WR16(p + 0u, 16u);
        n--;
        p += 20u;
    } while (n != 0u);

    /* 6 records at s3+6, stride +16 */
    p = s3 + 6u;
    n = 6u;
    do {
        WR8(p - 3u, 3u);
        WR8(p + 1u, 124u);
        WR8(p - 2u, 128u);
        WR8(p - 1u, 128u);
        WR8(p + 0u, 128u);
        n--;
        p += 16u;
    } while (n != 0u);

    /* 206 records at s4+0x12, stride +20 */
    p = s4 + 0x12u;
    n = 206u;
    do {
        WR8(p - 0xFu, 4u);
        WR8(p - 0xBu, 100u);
        WR8(p - 0xEu, 128u);
        WR8(p - 0xDu, 128u);
        WR8(p - 0xCu, 128u);
        WR16(p - 2u, 12u);
        WR16(p + 0u, 16u);
        n--;
        p += 20u;
    } while (n != 0u);

    WR8(s0 + 0x3Fu, 0u);
    WR8(s0 + 0x3Eu, 0u);
    WR8(s0 + 0x3Du, 0u);
    WR8(s0 + 0x3Cu, 0u);
    m->call5(m->ctx, 0x800399F0u, s0, 0u, 1u, 31u, 0u);
    return m->call5(m->ctx, 0x800399F0u, s0 + 12u, 0u, 1u, 31u, 0u);
}
