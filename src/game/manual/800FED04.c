#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FED04(const GuestMem *m, uint32_t a0) {
    uint32_t s0 = a0;
    uint32_t s1 = RD32(s0 + 0x80u);
    uint32_t s2 = RD32(s1 + 0x174u);
    uint32_t s3 = RD32(s1 + 0x17Cu);
    uint32_t s4 = RD32(s1 + 0x180u);
    uint32_t p;
    uint32_t t;
    uint32_t v;
    int32_t sv;
    uint16_t c16;

    /* 800FED2C..800FED3C: byte at 0x80066CA2 nonzero: lh of +0x16A; a value <= 0 also clears the halfword at +0x100 */
    if (RD8(0x80066CA2u) != 0u) {
        sv = RDS16(s0 + 0x16Au);
        if (sv <= 0) {
            WR16(s0 + 0x100u, 0u);
        }
        return (uint32_t)sv;
    }

    /* 800FED5C..800FEE18: byte+0x1FD of s1 selects the first block (A) or the second (B) */
    if (RD8(s1 + 0x1FDu) == 0u) {
        /* block A */
        if (RD8(s0 + 4u) == 0u) {
            if (RD8(s0 + 3u) == 0u) {
                /* 800FED98: (halfword +0x1CC of s1 & 0x80) == 0 sets byte+4 = 1 */
                if ((RD16(s1 + 0x1CCu) & 0x80u) == 0u) {
                    WR8(s0 + 4u, 1u);
                }
            } else {
                /* 800FEDAC..800FEDC8: byte+3 == 1 tests 0x10, otherwise 0x20 */
                if (RD8(s0 + 3u) == 1u) {
                    t = RD16(s1 + 0x1CCu) & 0x10u;
                } else {
                    t = RD16(s1 + 0x1CCu) & 0x20u;
                }
                if (t == 0u) {
                    WR8(s0 + 4u, 1u);
                }
            }
        }
    } else {
        /* block B, 800FEDD8..800FEE18: source record is s4 when byte+0x205 of s3 is nonzero, else s3 */
        p = (RD8(s3 + 0x205u) != 0u) ? s4 : s3;
        if (((RD16(p + 0x6Au) - RD16(s0 + 0x6Au) + 24u) & 0xFFFFu) < 49u) {
            WR8(s0 + 4u, 1u);
        }
    }

    /* 800FEE1C: jal 80026A04 with $a0 = s0 */
    m->call4(m->ctx, 0x80026A04u, s0, 0u, 0u, 0u);

    if (RD8(s0 + 6u) == 0u) {
        /* 800FEE34..800FEE74 */
        if (RD8(s1 + 6u) == 1u || RD8(s2 + 6u) == 1u) {
            goto L_2390;
        }
        /* 800FEE54..800FEE6C */
        if (RD8(s1 + 5u) == 1u && RD8(s2 + 5u) == 1u) {
            goto L_98;
        }
        goto L_2390;
    }
    /* 800FEEDC..800FEF30 */
    if (RD8(s1 + 6u) != 0u) {
        goto L_2390;
    }
    if (RD8(s2 + 6u) != 0u) {
        goto L_2390;
    }
    if (RDS16(s0 + 0x16Au) <= 0) {
        goto L_2390;
    }
    /* 800FEF30..800FEF50 */
    if (RD8(s1 + 5u) != 1u) {
        goto L_2390;
    }
    if (RD8(s2 + 5u) != 1u) {
        goto L_2390;
    }
    goto L_74;

L_98:
    /* 800FEE98..800FEEB0: if the word at +0x74 is nonzero, jal 800B428C with $a0 = s0 and return its $v0 */
    if (RD32(s0 + 0x74u) != 0u) {
        return m->call4(m->ctx, 0x800B428Cu, s0, 0u, 0u, 0u);
    }
    /* 800FEEB8..800FEED8: byte+6 = 1; word +0x70 = dword_800FFC98[byte+3 of s0]; that word is returned */
    WR8(s0 + 6u, 1u);
    v = RD32(0x800FFC98u + 4u * RD8(s0 + 3u));
    WR32(s0 + 0x70u, v);
    return v;

L_2390:
    /* 800FEE78..800FEE94 (and the same tail from 800FEF10, 800FEF54): byte+5 = 2; jal 80025A50 ($a0 = s0, $a1 = 2);
     * then jal 800268BC with $a1 = word at 0x80102390; its $v0 is returned */
    WR8(s0 + 5u, 2u);
    m->call4(m->ctx, 0x80025A50u, s0, 2u, 0u, 0u);
    return m->call4(m->ctx, 0x800268BCu, s0, RD32(0x80102390u), 0u, 0u);

L_74:
    /* 800FEF74..800FEFBC: when word +0x108 is nonzero: clear it, byte+8 = 1, byte+0xB = (halfword +0x6A of s0 is not
     * less than that of the record) ? 1 : 0, then jal 800268BC with $a1 = word at 0x80102394 */
    v = RD32(s0 + 0x108u);
    if (v != 0u) {
        sv = RDS16(s0 + 0x6Au);
        WR32(s0 + 0x108u, 0u);
        WR8(s0 + 8u, 1u);
        WR8(s0 + 0xBu, (sv < RDS16(v + 0x6Au)) ? 0u : 1u);
        m->call4(m->ctx, 0x800268BCu, s0, RD32(0x80102394u), 0u, 0u);
    }

    /* 800FEFC0..800FEFFC: byte+8 nonzero: jal 80026A04; a signed byte at +0x4D other than -1 returns 0x80100000
     * (the lui in the branch delay slot); else byte+8 = 0 and jal 800268BC with $a1 = word at 0x80102398 */
    if (RD8(s0 + 8u) != 0u) {
        m->call4(m->ctx, 0x80026A04u, s0, 0u, 0u, 0u);
        if (RDS8(s0 + 0x4Du) != -1) {
            return 0x80100000u;
        }
        WR8(s0 + 8u, 0u);
        return m->call4(m->ctx, 0x800268BCu, s0, RD32(0x80102398u), 0u, 0u);
    }

    /* 800FF000..800FF020: if byte+7 was 0 it becomes 1, word +0x74 = 0x10000, word +0x7C = -2048 */
    if (RD8(s0 + 7u) == 0u) {
        WR8(s0 + 7u, 1u);
        WR32(s0 + 0x74u, 0x10000u);
        WR32(s0 + 0x7Cu, 0xFFFFF800u);
    }
    /* 800FF024..800FF02C: only byte+7 == 1 continues; otherwise go to 800FF1A4 */
    if (RD8(s0 + 7u) != 1u) {
        goto L_ff1a4;
    }
    /* 800FF034: jal 800B42F4 with $a0 = s0 */
    m->call4(m->ctx, 0x800B42F4u, s0, 0u, 0u, 0u);
    /* 800FF03C..800FF084: word X = +0x7C, Y = +0x74.
     * X == -2048 and Y == 0xFFFF0000: X = 2048.  X == 2048 and Y == 0x10000: X = -2048.  Otherwise unchanged. */
    {
        uint32_t x = RD32(s0 + 0x7Cu);
        if (x != 0xFFFFF800u) {
            if (x == 0x800u && RD32(s0 + 0x74u) == 0x10000u) {
                WR32(s0 + 0x7Cu, 0xFFFFF800u);
            }
        } else if (RD32(s0 + 0x74u) == 0xFFFF0000u) {
            WR32(s0 + 0x7Cu, 0x800u);
        }
    }
    /* 800FF088..800FF0D8 (byte+0x205 of s3 zero) and 800FF0DC..800FF11C (nonzero, record s4): if the 16-bit difference
     * plus 16 is at least 33, the word +0x68 gets +0x70 when (halfword +0x6A of s0 << 16) < (record halfword << 16),
     * else -0x70 */
    p = (RD8(s3 + 0x205u) != 0u) ? s4 : s3;
    if (((RD16(p + 0x6Au) - RD16(s0 + 0x6Au) + 16u) & 0xFFFFu) >= 33u) {
        if ((int32_t)((uint32_t)RD16(s0 + 0x6Au) << 16) < (int32_t)((uint32_t)RD16(p + 0x6Au) << 16)) {
            WR32(s0 + 0x68u, RD32(s0 + 0x68u) + RD32(s0 + 0x70u));
        } else {
            WR32(s0 + 0x68u, RD32(s0 + 0x68u) - RD32(s0 + 0x70u));
        }
    }

    /* 800FF134..800FF15C: byte+4 == 0: halfword +0xC decremented and stored; returns its sign extension unless it is -1 */
    if (RD8(s0 + 4u) == 0u) {
        c16 = (uint16_t)(RD16(s0 + 0xCu) - 1u);
        sv = (int16_t)c16;
        WR16(s0 + 0xCu, c16);
        if (sv != -1) {
            return (uint32_t)sv;
        }
    }
    /* 800FF164..800FF190: jal 80024104 with $a0 = s1, $a1 = 296 (byte+1 cleared in its delay slot); byte+7 incremented;
     * halfword +0xC = 9; jal 800268BC with $a1 = word at 0x8010238C */
    WR8(s0 + 1u, 0u);
    m->call4(m->ctx, 0x80024104u, s1, 296u, 0u, 0u);
    v = RD8(s0 + 7u) + 1u;
    WR16(s0 + 0xCu, 9u);
    WR8(s0 + 7u, v);
    m->call4(m->ctx, 0x800268BCu, s0, RD32(0x8010238Cu), 0u, 0u);
    /* 800FF194..800FF19C: jal 800FF2AC with $a0 = s0 (not in the manual queue); its $v0 is returned */
    return m->call4(m->ctx, 0x800FF2ACu, s0, 0u, 0u, 0u);

    /* 800FF1A4..800FF1D0: halfword +0xC decremented and stored; if its sign extension is not -1 the result is 3;
     * else byte+5 = 3 and jal 80025A50 with $a0 = s0, $a1 = 2 returns its $v0 */
L_ff1a4:
    c16 = (uint16_t)(RD16(s0 + 0xCu) - 1u);
    sv = (int16_t)c16;
    WR16(s0 + 0xCu, c16);
    if (sv != -1) {
        return 3u;
    }
    WR8(s0 + 5u, 3u);
    return m->call4(m->ctx, 0x80025A50u, s0, 2u, 0u, 0u);
}
