#include "manual.h"
#include "hex2c_rt.h"

/* 800FEA50: a0 = actor, a1 = byte (shifted to bits 31..24). Four times: pick a free child slot with 80025548(
 * lb(actor.+0x136) + 4, 0, sign-extended byte, $a3 on the first pass only); when it is nonzero store the actor's
 * pointer at +0x80, the loop index at +4 and halfwords +0x6A and +0x6E from the actor. Returns 0 (the loop's last
 * compare). */
uint32_t man_800FEA50(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t s2 = a0;
    uint32_t s3 = a1 << 24;
    uint32_t s0 = 0u;
    uint32_t s1, v6, result;

    do {
        s1 = RD32(s2 + 0x80u);
        v6 = m->call4(m->ctx, 0x80025548u, (uint32_t)(RDS8(s1 + 0x136u) + 4), 0u,
                      (uint32_t)((int32_t)s3 >> 24), s0 == 0u ? a3 : 0u);
        if (v6 != 0u) {
            WR32(v6 + 0x80u, s1);
            WR8(v6 + 4u, s0);
            WR16(v6 + 0x6Au, RD16(s2 + 0x6Au));
            WR16(v6 + 0x6Eu, RD16(s2 + 0x6Eu));
        }
        s0 = (s0 + 1u) & 0xFFu;
        result = (uint32_t)(s0 < 4u);
    } while (result != 0u);
    return result;
}
