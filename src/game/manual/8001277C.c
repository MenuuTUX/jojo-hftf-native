#include "manual.h"
#include "hex2c_rt.h"

/* 8001277C: a0 = value record (length byte +6, zero-strip flag +7), a1 = value, a2 = mode (low byte),
 * a3 = table record (indexes at +0x3C/+0x3D/+0x3F, counters at +0x40 (halfword) / +0x42 / +0x44, sources +0x24/+0x26).
 * 1. Writes the hex digits of a1 into byte_80073D00 (length 1, 2, 4 or 8 from a0[6]; other lengths write nothing).
 * 2. If a0[7] != 0 and the first digit is '0', blanks the leading zeros (the last digit is kept).
 * 3. Mode 0: table 0x80073D10 entry, then the digits go to word_80074080 (halfword counter at a3+0x40, stop at 198).
 *    Mode 1: table 0x80073EA0, digits to word_80074210 (byte counter a3+0x42, stop at 111).
 *    Mode 3: table 0x80073FB0, digits to word_80074300 through the 0x800540E0 halfword table (byte counter a3+0x44, stop at 111).
 *    Any other mode: nothing more, returns 3.
 * Returns the $v0 that the original leaves at jr ra: 0 at the normal end, 2*counter when the counter limit stops the copy
 * (396 for mode 0, 222 for mode 1 and 3), 3 for other modes. */
uint32_t man_8001277C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const uint32_t hex = 0x80055E90u;   /* a0123456789abcd_1 */
    const uint32_t d00 = 0x80073D00u;   /* byte_80073D00 */
    const uint32_t rec = a0;            /* $t2 */
    const uint32_t tab = a3;            /* $a3 */
    uint32_t v0 = 0u;
    uint32_t v1;
    uint32_t len;
    uint32_t t0 = 0u;
    uint32_t p;
    uint32_t c;
    uint32_t cur;
    uint32_t cnt;
    uint32_t idx;
    uint32_t e;
    uint32_t addr;
    uint32_t add;

    /* 80012784..80012828, 80012894..800128F0: digits of a1 into the byte_80073D00 buffer */
    len = RD8(rec + 6u);
    v1 = len;
    if (v1 == 2u) {
        WR8(d00, RD8(hex + ((a1 & 0xF0u) >> 4)));
        v0 = RD8(hex + (a1 & 0xFu));
        WR8(d00 + 2u, 0u);
        WR8(d00 + 1u, v0);
    } else if (v1 < 3u) {
        if (v1 == 1u) {
            v0 = RD8(hex + (a1 & 0xFu));
            WR8(d00 + 1u, 0u);
            WR8(d00, v0);
        }
    } else if (v1 == 4u) {
        WR8(d00, RD8(hex + ((a1 & 0xF000u) >> 12)));
        WR8(d00 + 1u, RD8(hex + ((a1 & 0xF00u) >> 8)));
        WR8(d00 + 2u, RD8(hex + ((a1 & 0xF0u) >> 4)));
        v0 = RD8(hex + (a1 & 0xFu));
        WR8(d00 + 4u, 0u);
        WR8(d00 + 3u, v0);
    } else if (v1 == 8u) {
        WR8(d00, RD8(hex + (a1 >> 28)));
        WR8(d00 + 1u, RD8(hex + ((a1 >> 24) & 0xFu)));
        WR8(d00 + 2u, RD8(hex + ((a1 >> 20) & 0xFu)));
        WR8(d00 + 3u, RD8(hex + ((a1 >> 16) & 0xFu)));
        WR8(d00 + 4u, RD8(hex + ((a1 >> 12) & 0xFu)));
        WR8(d00 + 5u, RD8(hex + ((a1 >> 8) & 0xFu)));
        WR8(d00 + 6u, RD8(hex + ((a1 >> 4) & 0xFu)));
        v0 = RD8(hex + (a1 & 0xFu));
        WR8(d00 + 8u, 0u);
        WR8(d00 + 7u, v0);
    }

    /* 80012950..800129A8: leading-zero blanking when a0[7] != 0 and the first digit is '0' */
    if (RD8(rec + 7u) != 0u) {
        p = d00;
        if (RD8(p) == 48u) {
            do {
                if (t0 != RD8(rec + 6u) - 1u) {
                    WR8(p, 32u);
                }
                p = p + 1u;
                t0 = t0 + 1u;
            } while (RD8(p) == 48u);
        }
    }

    /* 800129AC..800129B4: mode = low byte of a2 */
    if ((a2 & 0xFFu) == 0u) {
        /* 800129B8..80012A24: table entry at 0x80073D10 (8 bytes), then the copy into word_80074080 */
        idx = RD8(tab + 0x3Cu);
        e = 0x80073D10u + idx * 8u;
        WR8(e, RD8(tab + 0x40u));
        idx = RD8(tab + 0x3Cu);
        e = 0x80073D10u + idx * 8u;
        WR8(e + 3u, RD8(rec + 4u));
        idx = RD8(tab + 0x3Cu);
        e = 0x80073D10u + idx * 8u;
        WR16(e + 4u, RD16(tab + 0x24u));
        idx = RD8(tab + 0x3Cu);
        e = 0x80073D10u + idx * 8u;
        WR16(e + 6u, RD16(tab + 0x26u));

        v0 = RD8(d00);
        if (v0 != 0u) {
            p = d00;
            for (;;) {
                /* 80012A38..80012A44: halfword counter a3+0x40; 198 ends the copy with $v0 = 2 * 198 */
                cnt = RD16(tab + 0x40u);
                if (cnt == 198u) {
                    return cnt << 1;
                }
                /* 80012A48..80012A90: word_80074080[cnt] = char, then shift it by 16 / 32 / 48 unless it is a space */
                c = RD8(p);
                addr = 0x80074080u + (cnt << 1);
                WR16(addr, c);
                addr = 0x80074080u + (RD16(tab + 0x40u) << 1);   /* 80012A54: the counter is read again */
                cur = RD16(addr);
                if (cur != 32u) {
                    add = (cur < 49u) ? 16u : ((cur < 65u) ? 32u : 48u);
                    WR16(addr, cur + add);
                }
                /* 80012A94..80012AA4: counter++ and next character */
                WR16(tab + 0x40u, RD16(tab + 0x40u) + 1u);
                p = p + 1u;
                v0 = RD8(p);
                if (v0 == 0u) {
                    break;
                }
            }
        }
        return v0;
    }

    if ((a2 & 0xFFu) == 1u) {
        /* 80012AC8..80012B28: table entry at 0x80073EA0, then the copy into word_80074210 */
        idx = RD8(tab + 0x3Du);
        e = 0x80073EA0u + idx * 8u;
        WR8(e, RD8(tab + 0x42u));
        idx = RD8(tab + 0x3Du);
        e = 0x80073EA0u + idx * 8u;
        WR8(e + 3u, RD8(rec + 4u));
        idx = RD8(tab + 0x3Du);
        e = 0x80073EA0u + idx * 8u;
        WR16(e + 4u, RD16(tab + 0x24u));
        idx = RD8(tab + 0x3Du);
        e = 0x80073EA0u + idx * 8u;
        WR16(e + 6u, RD16(tab + 0x26u));

        v0 = RD8(d00);
        if (v0 != 0u) {
            p = d00;
            for (;;) {
                /* 80012B48..80012B54: byte counter a3+0x42; 111 ends the copy with $v0 = 2 * 111 */
                cnt = RD8(tab + 0x42u);
                if (cnt == 111u) {
                    return cnt << 1;
                }
                /* 80012B58..80012B90: word_80074210[cnt] = char, then shift by 144 / 192 / 240 unless it is a space */
                c = RD8(p);
                addr = 0x80074210u + (cnt << 1);
                WR16(addr, c);
                addr = 0x80074210u + (RD8(tab + 0x42u) << 1);   /* 80012B64: the counter is read again */
                cur = RD16(addr);
                if (cur != 32u) {
                    add = (cur < 49u) ? 144u : ((cur < 65u) ? 192u : 240u);
                    WR16(addr, cur + add);
                }
                /* 80012BA4..80012BB4: counter++ and next character */
                WR8(tab + 0x42u, RD8(tab + 0x42u) + 1u);
                p = p + 1u;
                v0 = RD8(p);
                if (v0 == 0u) {
                    break;
                }
            }
        }
        return v0;
    }

    if ((a2 & 0xFFu) == 3u) {
        /* 80012BD4..80012C2C: table entry at 0x80073FB0, then the copy into word_80074300 */
        idx = RD8(tab + 0x3Fu);
        e = 0x80073FB0u + idx * 8u;
        WR8(e, RD8(tab + 0x44u));
        idx = RD8(tab + 0x3Fu);
        e = 0x80073FB0u + idx * 8u;
        WR8(e + 3u, RD8(rec + 4u));
        idx = RD8(tab + 0x3Fu);
        e = 0x80073FB0u + idx * 8u;
        WR16(e + 4u, RD16(tab + 0x24u));
        idx = RD8(tab + 0x3Fu);
        e = 0x80073FB0u + idx * 8u;
        WR16(e + 6u, RD16(tab + 0x26u));

        v0 = RD8(d00);
        if (v0 != 0u) {
            p = d00;
            for (;;) {
                /* 80012C48..80012C68: byte counter a3+0x44; 111 ends the copy with $v0 = 2 * 111 */
                cnt = RD8(tab + 0x44u);
                if (cnt == 111u) {
                    return cnt << 1;
                }
                /* 80012C70..80012CBC: word_80074300[cnt] = char, then if it is not a space take the 0x800540E0 halfword
                 * table at (char - 33) & 0xFFFF */
                c = RD8(p);
                addr = 0x80074300u + (cnt << 1);
                WR16(addr, c);
                addr = 0x80074300u + (RD8(tab + 0x44u) << 1);   /* 80012C7C: the counter is read again */
                cur = RD16(addr);
                if (cur != 32u) {
                    WR16(addr, RD16(0x800540E0u + 2u * ((RD8(p) - 33u) & 0xFFFFu)));
                }
                /* 80012CC0..80012CD8: counter++ and next character; the loop ends on a zero character with $v0 = 0 */
                WR8(tab + 0x44u, RD8(tab + 0x44u) + 1u);
                p = p + 1u;
                v0 = RD8(p);
                if (v0 == 0u) {
                    break;
                }
            }
        }
        return v0;
    }

    /* 80012BCC..80012BD0: any other mode returns 3 */
    return 3u;
}
