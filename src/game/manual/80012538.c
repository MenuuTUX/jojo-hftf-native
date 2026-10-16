#include "manual.h"
#include "hex2c_rt.h"

/* 80012538 (TEXT). $a0 = P (u16 x at +0, u16 y at +2, u8 at +4, u8 type at +6), $a1 = S (string pointer).
 * G = 0x8006D1F0 is the $s0 base: G+0x24 halfword x, G+0x26 halfword y, G+0x3F slot index k, G+0x44 text position j.
 * Slot k fields: 0x80073FB0+8k (start), +2 (flag), +3 (type), +4 (x), +6 (y), +1 (length).
 * Text buffer: 0x80074300 (halfwords, max 103 entries). Font table: 0x800540E0 (halfwords, indexed by char-33). */
uint32_t man_80012538(const GuestMem *m, uint32_t a0, uint32_t a1) {
    const uint32_t G = 0x8006D1F0u;
    uint32_t k;
    uint32_t j;
    uint32_t c;

    if (RD8(G + 0x3Fu) == 25u)
        return 25u;

    WR16(G + 0x24u, RD16(a0));
    WR16(G + 0x26u, 224u - RD16(a0 + 2u));
    k = RD8(G + 0x3Fu);
    WR8(0x80073FB0u + 8u * k, RD8(G + 0x44u));
    WR8(0x80073FB3u + 8u * k, RD8(a0 + 4u));
    WR8(0x80073FB2u + 8u * k, 0u);
    WR16(0x80073FB4u + 8u * k, RD16(G + 0x24u));
    WR16(0x80073FB6u + 8u * k, RD16(G + 0x26u));

    if (RD8(a0 + 6u) == 0u) {
        /* text path: the first character is stored before the zero test */
        WR8(0x80073FB2u + 8u * k, 0u);
        j = RD8(G + 0x44u);
        WR16(0x80074300u + 2u * j, RD8(a1));
        c = RD8(a1);
        if (c != 0u) {
            for (;;) {
                j = RD8(G + 0x44u);
                if (j == 103u)
                    break;
                WR16(0x80074300u + 2u * j, RD8(a1));
                j = RD8(G + 0x44u);
                if (RD16(0x80074300u + 2u * j) != 32u) {
                    uint32_t idx = (RD8(a1) - 33u) & 0xFFFFu;
                    WR16(0x80074300u + 2u * j, RD16(0x800540E0u + 2u * idx));
                }
                WR8(G + 0x44u, RD8(G + 0x44u) + 1u);
                a1 = a1 + 1u;
                if (RD8(a1) == 0u)
                    break;
            }
        }
    } else if (RD8(a0 + 6u) == 10u) {
        WR8(0x80073FB2u + 8u * k, RD8(a0 + 6u));
        j = RD8(G + 0x44u);
        if (j != 103u) {
            WR16(0x80074300u + 2u * j, RD16(a1));
            WR8(G + 0x44u, RD8(G + 0x44u) + 1u);
        }
    } else {
        m->call4(m->ctx, 0x8001277Cu, a0, a1, 3u, G);
    }

    /* common tail: length of the run, advance the slot index, return the new index */
    k = RD8(G + 0x3Fu);
    WR8(0x80073FB1u + 8u * k, RD8(G + 0x44u) - RD8(0x80073FB0u + 8u * k));
    WR8(G + 0x3Fu, k + 1u);
    return k + 1u;
}
