#include "manual.h"
#include "hex2c_rt.h"

/* 800277F4: never returns. Per frame: for each of the 8 slots (18 bytes each from 0x8006D238): when the slot's flag byte is
 * set, decrement the counter byte at +4; at zero, copy the byte at +5 back into it and call 8002788C(slot). Then unless
 * the word at 0x8006CED4 is nonzero call 80010948; then 80010914(1). Arguments are passed on unchanged until the first call
 * (e0..e3); after any call they are the callee's leftovers and pass as 0. */
uint32_t man_800277F4(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t e0 = a0, e1 = a1, e2 = a2, e3 = a3;
    uint32_t s0, s1, s2, cnt;

    for (;;) {
        s1 = 0x8006D238u;
        s2 = 8u;
        s0 = s1 + 4u;
        do {
            if (RD8(s1) != 0u) {
                cnt = (uint32_t)(RD8(s0) - 1u) & 0xFFu;
                WR8(s0, cnt);
                if (cnt == 0u) {
                    WR8(s0, RD8(s0 + 1u));
                    m->call4(m->ctx, 0x8002788Cu, s1, e1, e2, e3);
                    e0 = 0u; e1 = 0u; e2 = 0u; e3 = 0u;
                }
            }
            s0 += 18u;
            s2 -= 1u;
            s1 += 18u;
        } while (s2 != 0u);
        if (RD32(0x8006CED4u) == 0u) {
            m->call4(m->ctx, 0x80010948u, e0, e1, e2, e3);
            e0 = 0u; e1 = 0u; e2 = 0u; e3 = 0u;
        }
        m->call4(m->ctx, 0x80010914u, 1u, e1, e2, e3);
    }
}
