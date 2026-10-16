#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800121F0(const GuestMem *m, uint32_t a0, uint32_t a1) {
    const uint32_t g = 0x8006D1F0u;   /* $s0 */
    uint32_t rec = a0;                /* $a2: record pointer (fields at +0, +2, +4, +6) */
    uint32_t str = a1;                /* $a1: text pointer, advanced by the loop */
    uint32_t idx;
    uint32_t ent;
    uint32_t p;
    uint32_t ea;
    uint32_t c;
    uint32_t v;

    /* 80012204..8001220C: if byte_8006D22D == 30, return 30 untouched */
    if (RD8(g + 0x3Du) == 30u) {
        return 30u;
    }

    /* 80012218..80012230: word_8006D214 = rec[0]; word_8006D216 = 224 - rec[2] */
    WR16(g + 0x24u, RD16(rec + 0u));
    WR16(g + 0x26u, 224u - RD16(rec + 2u));

    /* 80012234..80012280: fill entry idx of the table at 0x80073EA0 (8 bytes each) */
    idx = RD8(g + 0x3Du);
    ent = 0x80073EA0u + idx * 8u;
    WR8(ent + 0u, RD8(g + 0x42u));
    WR8(ent + 3u, RD8(rec + 4u));
    WR16(ent + 4u, RD16(g + 0x24u));
    WR16(ent + 6u, RD16(g + 0x26u));

    if (RD8(rec + 6u) != 0u) {
        /* 8001234C..80012358: rec[6] != 0 calls 8001277C with a2 = 1 */
        m->call4(m->ctx, 0x8001277Cu, rec, str, 1u, g);
    } else {
        /* 80012294..80012348: copy the string into the table at 0x80074210 */
        while (RD8(str) != 0u) {
            p = RD8(g + 0x42u);
            if (p == 111u) {
                break;
            }
            /* 800122B8..800122D4: word_80074210[p] = (uint8_t)*str */
            WR16(0x80074210u + p * 2u, RD8(str));
            ea = 0x80074210u + p * 2u;
            c = RD16(ea);
            /* 800122DC..80012320: space stays, others shift by a per-range amount */
            if (c != 32u) {
                if (c >= 241u) {
                    v = c + 769u;
                } else if (c < 49u) {
                    v = c + 144u;
                } else if (c < 65u) {
                    v = c + 192u;
                } else if (c < 81u) {
                    v = c + 240u;
                } else {
                    v = c + 288u;
                }
                WR16(ea, v);
            }
            /* 80012324..80012338: byte_8006D232++ (stored as a byte), str++ */
            WR8(g + 0x42u, RD8(g + 0x42u) + 1u);
            str = str + 1u;
        }
    }

    /* 8001235C..80012378: byte_80073EA1[idx] = (uint8_t)(byte_8006D232 - byte_80073EA0[idx]) */
    idx = RD8(g + 0x3Du);
    ent = 0x80073EA0u + idx * 8u;
    WR8(ent + 1u, RD8(g + 0x42u) - RD8(ent));

    /* 80012384..80012390: byte_8006D22D++ ; $v0 = old value + 1 is the return value */
    idx = RD8(g + 0x3Du);
    WR8(g + 0x3Du, idx + 1u);
    return idx + 1u;
}
