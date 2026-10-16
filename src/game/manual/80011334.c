#include "manual.h"
#include "hex2c_rt.h"

/* 80011334 (TEXT). $a0 = index (byte, andi 0xff), $a1 = flag (byte, andi 0xff).
 * $a2 and $a3 are never written here; they are passed on unchanged to 8001E29C / 8001E2C4 / 800109F0
 * (the callees and their callees may read them), so they are parameters too.
 * Index 0: flag set -> call 8001E29C; table 800DF154. Index 1: flag set -> 8001E2C4; table 800DF0CC.
 * Index 2: flag set -> 8001E2C4; table 800DF95C. Index 3: flag set -> 8001E2C4 with $a0 = 800E0000; table 800E0350.
 * Any other index returns a1 & 0xFF (the delay slot of the bne at 800113B8 sets $v0 to it) without calling anything.
 * Every indexed path ends in a call to 800109F0 (table) and returns its $v0. */
uint32_t man_80011334(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t x = a0 & 0xFFu;
    uint32_t flag = a1 & 0xFFu;
    uint32_t tbl;

    if (x == 0u) {
        if (flag != 0u)
            m->call4(m->ctx, 0x8001E29Cu, 0u, a1, a2, a3);
        tbl = 0x800DF154u;
    } else if (x == 1u) {
        if (flag != 0u)
            m->call4(m->ctx, 0x8001E2C4u, 1u, a1, a2, a3);
        tbl = 0x800DF0CCu;
    } else if (x == 2u) {
        if (flag != 0u)
            m->call4(m->ctx, 0x8001E2C4u, 2u, a1, a2, a3);
        tbl = 0x800DF95Cu;
    } else if (x == 3u) {
        if (flag != 0u)
            m->call4(m->ctx, 0x8001E2C4u, 0x800E0000u, a1, a2, a3);
        tbl = 0x800E0350u;
    } else {
        return flag;
    }

    return m->call4(m->ctx, 0x800109F0u, tbl, a1, a2, a3);
}
