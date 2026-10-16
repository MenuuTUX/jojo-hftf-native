#include "manual.h"
#include "hex2c_rt.h"

/* 8001F2E4: for the 24 entries of the list at *(0x80073B38)+920 going down by 40 bytes, when bytes 0 and 1 are set, calls
 * 0x80020B34(a0, entry) then 0x8001FF94(a0). Returns $v0 as the original leaves it: the flag byte at 0x80066C6F when it is 0, the
 * list base when it is -920, else the final loop test (0).
 * Hand-written so the two callees go through guest calls (m->call4) instead of direct native calls: the translated version runs
 * the whole loop (up to ~240,000 guest cycles) as one native step with no interrupt point, and the fight then differs from fight
 * state 575 (phase2-358); its code was clean on 15 fight states that ran the loop (phase2-368). */
uint32_t man_8001F2E4(const GuestMem *m, uint32_t a0) {
    uint32_t sp = m->sp_get(m->ctx) - 0x20u;
    uint32_t v0 = RD8(0x80066C6Fu), s0;
    int i;
    m->sp_set(m->ctx, sp);
    if (v0 != 0) {
        v0 = RD32(0x80073B38u);
        s0 = v0 + 920u;
        if (s0 != 0) {
            for (i = 0; i < 24; ++i, s0 -= 40u) {
                if (RD8(s0) != 0 && RD8(s0 + 1u) != 0) {
                    m->call4(m->ctx, 0x80020B34u, a0, s0, 0, 0);
                    m->call4(m->ctx, 0x8001FF94u, a0, 0, 0, 0);
                }
            }
            v0 = 0;
        }
    }
    m->sp_set(m->ctx, sp + 0x20u);
    return v0;
}
