#include "manual.h"
#include "hex2c_rt.h"

/* 8002074C: unpacks one 128-byte block from the guest stream at $a0 into the scratchpad buffer 0x1F800380.
 * A flag byte is read at the start of each group of 8 steps ($t1 & 7 == 0; $t1 counts 128 down to 1), and its bits are
 * taken low first: bit 0 set = back-reference, clear = literal byte. The flag byte read at entry ($t0) is never used:
 * on the first pass $t1 = 128, so the flag byte is reloaded (80020768) before the first use (80020770 overwrites the
 * entry-value $v0 computed at 80020764). Literal: one byte, $a3 -= 1. Back-reference: header byte with high nibble h and
 * low nibble l. h != 0: copies l bytes from out - h, one byte at a time (overlap allowed), $a3 -= l. h == 0: the next byte
 * is repeated l + 1 times, $a3 -= l + 1. The loop ends when $a3 (unsigned) is 0, tested at the bottom.
 * Returns $t0 & 0xFF of the last pass (the flag byte before its final shift). No calls. */
uint32_t man_8002074C(const GuestMem *m, uint32_t a0) {
    uint32_t src = a0;            /* $a0, the stream */
    uint32_t out = 0x1F800380u;   /* $a2 */
    uint32_t t0 = 0u;             /* flag byte; the entry value is dead (see above) */
    uint32_t t1 = 128u;           /* bit counter */
    uint32_t a3 = 128u;           /* bytes left */
    uint32_t v0 = 0u;

    do {
        if ((t1 & 7u) == 0u) {
            t0 = RD8(src);
            src += 1u;
        }
        if ((t0 & 1u) == 0u) {
            /* literal */
            a3 -= 1u;
            WR8(out, RD8(src));
            src += 1u;
            out += 1u;
        } else {
            uint32_t hdr = RD8(src);
            uint32_t lo = hdr & 0xFu;
            uint32_t hi = hdr >> 4;
            src += 1u;
            if (hi != 0u) {
                uint32_t from = out - hi;
                a3 -= lo;
                while (lo != 0u) {
                    WR8(out, RD8(from));
                    from += 1u;
                    out += 1u;
                    lo -= 1u;
                }
            } else {
                uint32_t val = RD8(src);
                uint32_t n = lo + 1u;
                src += 1u;
                a3 -= n;
                do {
                    WR8(out, val);
                    out += 1u;
                    n -= 1u;
                } while (n != 0u);
            }
        }
        v0 = t0 & 0xFFu;
        t0 = v0 >> 1;
        t1 -= 1u;
    } while (a3 != 0u);

    return v0;
}
