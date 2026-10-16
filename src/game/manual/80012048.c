#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_80012048(const GuestMem *m, uint32_t a0, uint32_t a1) {
    const uint32_t g = 0x8006D1F0u;   /* $s0 */
    uint32_t rec = a0;                /* $a2: record pointer (fields at +0, +2, +4, +6) */
    uint32_t str = a1;                /* $a1: text pointer, advanced by the loop */
    uint32_t idx;
    uint32_t ent;
    uint32_t p;
    uint32_t ea;
    uint32_t c;
    uint32_t v;

    /* 8001205C..80012064: if byte_8006D22C == 50, return 50 untouched */
    if (RD8(g + 0x3Cu) == 50u) {
        return 50u;
    }

    /* 80012070..80012088: word_8006D214 = rec[0]; word_8006D216 = 224 - rec[2] */
    WR16(g + 0x24u, RD16(rec + 0u));
    WR16(g + 0x26u, 224u - RD16(rec + 2u));

    /* 8001208C..800120D8: fill entry idx of the table at 0x80073D10 (8 bytes each) */
    idx = RD8(g + 0x3Cu);
    ent = 0x80073D10u + idx * 8u;
    WR8(ent + 0u, RD8(g + 0x40u));
    WR8(ent + 3u, RD8(rec + 4u));
    WR16(ent + 4u, RD16(g + 0x24u));
    WR16(ent + 6u, RD16(g + 0x26u));

    if (RD8(rec + 6u) != 0u) {
        /* 800120DC..800120E4: rec[6] != 0 calls 8001277C with a2 = 0 */
        m->call4(m->ctx, 0x8001277Cu, rec, str, 0u, g);
    } else {
        /* 800120EC..80012198: copy the string into the table at 0x80074080 */
        while (RD8(str) != 0u) {
            p = RD16(g + 0x40u);
            if (p == 198u) {
                break;
            }
            /* 80012108..80012124: word_80074080[p] = (uint8_t)*str */
            WR16(0x80074080u + p * 2u, RD8(str));
            ea = 0x80074080u + p * 2u;
            c = RD16(ea);
            /* 80012138..8001216C: space stays, other characters shift by 16/32/48/64 */
            if (c != 32u) {
                if (c < 49u) {
                    v = c + 16u;
                } else if (c < 65u) {
                    v = c + 32u;
                } else if (c < 81u) {
                    v = c + 48u;
                } else {
                    v = c + 64u;
                }
                WR16(ea, v);
            }
            /* 80012170..80012180: word_8006D230++ (stored as halfword), str++ */
            WR16(g + 0x40u, RD16(g + 0x40u) + 1u);
            str = str + 1u;
        }
    }

    /* 800121AC..800121C8: byte_80073D11[idx] = (uint8_t)(byte_8006D230 - byte_80073D10[idx]) */
    idx = RD8(g + 0x3Cu);
    ent = 0x80073D10u + idx * 8u;
    WR8(ent + 1u, RD8(g + 0x40u) - RD8(ent));

    /* 800121D0..800121DC: byte_8006D22C++ ; $v0 = old value + 1 is the return value */
    idx = RD8(g + 0x3Cu);
    WR8(g + 0x3Cu, idx + 1u);
    return idx + 1u;
}
