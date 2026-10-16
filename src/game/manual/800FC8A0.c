#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FC8A0(const GuestMem *m, uint32_t a0) {
    uint32_t s = a0;
    uint32_t idx;
    uint32_t t1;
    uint32_t p;
    uint32_t v5;
    uint32_t v6;
    uint32_t v7;
    uint32_t v8;
    uint32_t dx;
    uint32_t dy;
    uint32_t a1;

    /* 800FC8A4..800FC8B0: a3 = a0; byte 0x32 = 13 */
    WR8(s + 0x32u, 13u);
    /* 800FC8B4..800FC8B8: byte 0xB0 = 0x83 */
    WR8(s + 0xB0u, 0x83u);

    idx = RD8(s + 4u);
    /* 800FC8BC..800FC8E0: t1 = signed byte at 0x800F495C[idx]; word 0x78 = 0xFFFFE000 */
    t1 = (uint32_t)RDS8(0x800F495Cu + idx);
    WR32(s + 0x78u, 0xFFFFE000u);
    /* 800FC8E0..800FC8E8: p = 0x800F4918 + idx*16 */
    p = 0x800F4918u + idx * 16u;
    /* 800FC91C..800FC920: halfwords 0xF0 and 0xEE = t1 */
    WR16(s + 0xF0u, t1);
    WR16(s + 0xEEu, t1);
    /* 800FC908..800FC910: three words from the table row */
    v5 = RD32(p);
    v6 = RD32(p + 4u);
    v7 = RD32(p + 8u);
    v8 = RD8(s + 0xBu);
    WR32(s + 0x70u, v5);
    WR32(s + 0x74u, v6);
    WR32(s + 0x7Cu, v7);

    /* 800FC92C: beq a1,zero -> 800FC974 when byte 0xB == 0; otherwise the negated path */
    if (v8 != 0u) {
        /* 800FC934..800FC93C: word 0x70 = -v5 */
        WR32(s + 0x70u, 0u - v5);
        /* 800FC940..800FC95C: dx = halfword at 0x800F4908 + idx*4 */
        dx = RD16(0x800F4908u + idx * 4u);
        /* 800FC958..800FC968: word 0x78 = -(word 0x78) */
        WR32(s + 0x78u, 0u - RD32(s + 0x78u));
        /* 800FC964..800FC96C: halfword 0x6A = (halfword 0x6A) - dx */
        WR16(s + 0x6Au, RD16(s + 0x6Au) - dx);
    } else {
        /* 800FC974..800FC998: halfword 0x6A = (halfword 0x6A) + dx */
        dx = RD16(0x800F4908u + idx * 4u);
        WR16(s + 0x6Au, RD16(s + 0x6Au) + dx);
    }

    /* 800FC99C..800FC9C4: dy = halfword at 0x800F4908 + idx*4 + 2; halfword 0x6E += dy */
    dy = RD16(0x800F4908u + idx * 4u + 2u);
    WR16(s + 0x6Eu, RD16(s + 0x6Eu) + dy);

    /* 800FC9C0..800FC9E8: a1 = (byte 3 == 13) ? word 0x801022F4 : word 0x801022F8 */
    if (RD8(s + 3u) == 13u) {
        a1 = RD32(0x801022F4u);
    } else {
        a1 = RD32(0x801022F8u);
    }

    /* 800FC9E8: jal 800268BC with a0 = s, a1 as above, a2 = sign-extended t1, a3 = s; returns its $v0 */
    return m->call4(m->ctx, 0x800268BCu, s, a1, t1, s);
}
