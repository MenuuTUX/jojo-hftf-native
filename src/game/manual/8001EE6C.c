#include "manual.h"
#include "hex2c_rt.h"

/* 8001EE6C (TEXT). Reads no argument register (nargs 0). Returns 0x80080000 (the $v0 left by its last lui before jr ra).
 * Loop 1: i = 0..959, j = 0..1, p = 0x800797C8 + 16*i + 15360*j: SetSprt16(p), then bytes p+4..p+6 = 0x80 and halfword
 * p+14 = 0x7A00 (the stores run after the call, in that order).
 * Loop 2: i = 0..191, j = 0..1: byte 0x80080FC8 + 1536*j + 8*i + 3 = 1, word 0x80080FCC + 1536*j + 8*i =
 * 0xE1000020 | ((((((i >> 5) << 6) + 384) & 0x3FF) >> 6)). i is never negative, so the +31 path never runs.
 * Tail: ClearOTagR(0x801FB800, 256), ClearOTagR(0x801FBC70, 256), zero the word 0x8006D388, the word 0x80073BC0 and
 * the halfword 0x80081BC8. The callees get 0 in the argument registers the original does not set. */
uint32_t man_8001EE6C(const GuestMem *m) {
    uint32_t i;
    uint32_t j;

    for (i = 0; i < 960u; i++) {
        for (j = 0; j < 2u; j++) {
            uint32_t p = 0x800797C8u + 16u * i + 15360u * j;
            m->call4(m->ctx, 0x80039900u, p, 0u, 0u, 0u);
            WR8(p + 4u, 0x80u);
            WR8(p + 5u, 0x80u);
            WR8(p + 6u, 0x80u);
            WR16(p + 14u, 0x7A00u);
        }
    }

    for (i = 0; i < 192u; i++) {
        for (j = 0; j < 2u; j++) {
            uint32_t base = 0x80080FC8u + 1536u * j + 8u * i;
            uint32_t tile = (((((i >> 5) << 6) + 384u) & 0x3FFu) >> 6);
            WR8(base + 3u, 1u);
            WR32(base + 4u, 0xE1000020u | tile);
        }
    }

    m->call4(m->ctx, 0x80036588u, 0x801FB800u, 256u, 0u, 0u);
    m->call4(m->ctx, 0x80036588u, 0x801FBC70u, 256u, 0u, 0u);
    WR32(0x8006D388u, 0u);
    WR32(0x80073BC0u, 0u);
    WR16(0x80081BC8u, 0u);
    return 0x80080000u;
}
