#include "manual.h"
#include "hex2c_rt.h"

/* The block at 800FA86C..800FA898 (and again at 800FA910..800FA954): when byte_4E is nonzero, call 800FB7E8 twice
 * with byte_4E - 1 and then byte_4E + 1 as the signed third argument, then clear byte_4E. The first call passes $a3
 * on unchanged; the second gets 0 because the first clobbers it. Returns 1 when the block ran. */
static int fa738_clear4e(const GuestMem *m, uint32_t s0, uint32_t a3) {
    uint8_t t;
    if (RDS8(s0 + 0x4Eu) == 0) {
        return 0;
    }
    t = (uint8_t)(RD8(s0 + 0x4Eu) - 1u);
    WR8(s0 + 0x4Eu, t);
    m->call4(m->ctx, 0x800FB7E8u, s0, 8u, (uint32_t)(int32_t)(int8_t)t, a3);
    t = (uint8_t)(RD8(s0 + 0x4Eu) + 1u);
    m->call4(m->ctx, 0x800FB7E8u, s0, 8u, (uint32_t)(int32_t)(int8_t)t, 0u);
    WR8(s0 + 0x4Eu, 0u);
    return 1;
}

/* 800FA738 (PL02X): state machine on byte_7. $a1 is 279 on every path (delay slot of the first bne). The original's
 * tail is `jal 80026A04` with $a0 = object, so the return value is that callee's $v0.
 * Guest calls clobber $a1..$a3; they are set to 0 after each call (the convention of 80010354). */
uint32_t man_800FA738(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t s0 = a0;
    uint32_t st = RD8(s0 + 7u);
    uint32_t idx;
    uint32_t tb;

    a1 = 279u;

    if (st == 0u) {
        /* 800FA75C..800FA768: byte_7 = 1; byte_107 = 10 (delay slot); 80024104(obj, 279, a2, a3) */
        WR8(s0 + 7u, 1u);
        WR8(s0 + 0x107u, 10u);
        m->call4(m->ctx, 0x80024104u, s0, a1, a2, a3);
        a1 = 0u;
        a2 = 0u;
        a3 = 0u;

        /* 800FA778..800FA7C4: table at 0x800F48A4, 16-byte rows indexed by byte_207 */
        idx = RD8(s0 + 0x207u);
        tb = 0x800F48A4u + idx * 16u;
        a3 = 0x800F48A4u;
        WR32(s0 + 0x70u, RD32(tb + 0u));
        WR32(s0 + 0x78u, RD32(tb + 4u));
        WR32(s0 + 0x74u, RD32(tb + 8u));
        WR32(s0 + 0x7Cu, RD32(tb + 12u));
        a2 = RD8(s0 + 0xBu);
        if (a2 == 0u) {
            /* 800FA7D0..800FA7F8: with byte_B == 0, words 0 and 1 are negated */
            WR32(s0 + 0x70u, 0u - RD32(tb + 0u));
            WR32(s0 + 0x78u, 0u - RD32(tb + 4u));
        }

        /* 800FA800..800FA828 */
        WR32(s0 + 0x108u, 0u);
        idx = RD8(s0 + 0x207u);
        WR16(s0 + 0xCu, idx);
        a1 = (idx != 0u) ? 35u : 37u;
        m->call4(m->ctx, 0x800BB2A4u, s0, a1, a2, a3);
        a1 = 0u;
        a2 = 0u;
        a3 = 0u;

        /* 800FA830..800FA840 */
        m->call4(m->ctx, 0x800BB30Cu, s0, 0u, 0u, 0u);
        a1 = RD32(0x80102214u);

        /* 800FA9FC..800FAA04 and 800FAA24 */
        m->call4(m->ctx, 0x800268BCu, s0, a1, 0u, 0u);
        return m->call4(m->ctx, 0x80026A04u, s0, 0u, 0u, 0u);
    }

    if (st == 1u) {
        /* 800FA854..800FA898 */
        if (fa738_clear4e(m, s0, a3)) {
            a1 = 0u;
            a2 = 0u;
            a3 = 0u;
        }
        /* 800FA89C..800FA8BC: byte_4D == 2 moves byte_7 to 2 */
        if (RDS8(s0 + 0x4Du) == 2) {
            WR8(s0 + 7u, RD8(s0 + 7u) + 1u);
        }
        return m->call4(m->ctx, 0x80026A04u, s0, a1, a2, a3);
    }

    if (st == 2u) {
        /* 800FA8C8..800FA90C: byte_108 != 0 and halfword_C != 0: halfword_C--, 800BB2A4(51 or 50) */
        if (RD32(s0 + 0x108u) != 0u) {
            int16_t hv = RDS16(s0 + 0xCu);
            uint16_t lv = RD16(s0 + 0xCu);
            WR32(s0 + 0x108u, 0u);
            if (hv != 0) {
                uint16_t nv = (uint16_t)(lv - 1u);
                WR16(s0 + 0xCu, nv);
                a1 = (nv != 0u) ? 51u : 50u;
                m->call4(m->ctx, 0x800BB2A4u, s0, a1, a2, a3);
                a1 = 0u;
                a2 = 0u;
                a3 = 0u;
            }
        }

        /* 800FA910..800FA954 */
        if (fa738_clear4e(m, s0, a3)) {
            a1 = 0u;
            a2 = 0u;
            a3 = 0u;
        }

        /* 800FA958..800FA96C: word_70 != 0 calls 800B42D0 */
        if (RD32(s0 + 0x70u) != 0u) {
            m->call4(m->ctx, 0x800B42D0u, s0, a1, a2, a3);
            a1 = 0u;
            a2 = 0u;
            a3 = 0u;
        }

        /* 800FA970..800FA9A8 */
        m->call4(m->ctx, 0x800B42F4u, s0, a1, a2, a3);
        a1 = 0u;
        a2 = 0u;
        a3 = 0u;
        if (RDS16(s0 + 0x76u) < 3) {
            WR8(s0 + 7u, RD8(s0 + 7u) + 1u);
            a1 = RD32(0x80102218u);
            m->call4(m->ctx, 0x800268BCu, s0, a1, a2, a3);
            a1 = 0u;
            a2 = 0u;
            a3 = 0u;
        }
        return m->call4(m->ctx, 0x80026A04u, s0, a1, a2, a3);
    }

    if (st == 3u) {
        /* 800FA9B0..800FA9C8 */
        m->call4(m->ctx, 0x800B42F4u, s0, a1, a2, a3);
        a1 = 0u;
        a2 = 0u;
        a3 = 0u;
        if (RDS16(s0 + 0x6Eu) < 36) {
            /* 800FA9CC..800FA9FC: halfword_6E = 35, byte_7++, a1 = word at 0x8010219C + 4 * (byte_207 + 44) */
            WR16(s0 + 0x6Eu, 35u);
            WR8(s0 + 7u, RD8(s0 + 7u) + 1u);
            idx = RD8(s0 + 0x207u);
            a1 = RD32(0x8010219Cu + (idx + 44u) * 4u);
            m->call4(m->ctx, 0x800268BCu, s0, a1, a2, a3);
            a1 = 0u;
            a2 = 0u;
            a3 = 0u;
        }
        return m->call4(m->ctx, 0x80026A04u, s0, a1, a2, a3);
    }

    /* 800FAA0C..800FAA24: other states; byte_4D == -1 calls 800C1F38 */
    if (RDS8(s0 + 0x4Du) == -1) {
        m->call4(m->ctx, 0x800C1F38u, s0, a1, a2, a3);
        a1 = 0u;
        a2 = 0u;
        a3 = 0u;
    }
    return m->call4(m->ctx, 0x80026A04u, s0, a1, a2, a3);
}
