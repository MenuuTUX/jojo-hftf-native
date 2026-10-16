#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FD7E0(const GuestMem *m, uint32_t a0, uint32_t in_v0) {
    uint32_t s0 = a0;
    uint32_t v;

    /* 800FD7EC: bne v0,zero,800FD878 returns with $v0 unchanged */
    if (in_v0 != 0u) {
        return in_v0;
    }
    /* 800FD7F4..800FD808: lh/lhu of +0xC; a nonzero count is decremented, stored, and returned as (count - 1) */
    v = RD16(s0 + 0xCu) - 1u;
    if (RD16(s0 + 0xCu) != 0u) {
        WR16(s0 + 0xCu, v);
        return v;
    }
    /* 800FD80C..800FD82C: count was zero: byte+1 = 1, both 16-bit counters +1, then jal 800B428C with $a0 = s0 */
    WR8(s0 + 1u, 1u);
    WR16(s0 + 0xEEu, RD16(s0 + 0xEEu) + 1u);
    WR16(s0 + 0xF0u, RD16(s0 + 0xF0u) + 1u);
    m->call4(m->ctx, 0x800B428Cu, s0, 0u, 0u, 0u);
    /* 800FD834..800FD84C: if the reloaded +0xEE is 129 or more, both counters become 127 */
    if (RD16(s0 + 0xEEu) >= 129u) {
        WR16(s0 + 0xEEu, 127u);
        WR16(s0 + 0xF0u, 127u);
    }
    /* 800FD850: jal 80026A04 with $a0 = s0 */
    m->call4(m->ctx, 0x80026A04u, s0, 0u, 0u, 0u);
    /* 800FD858..800FD860: a signed byte at +0x4D other than -1 makes the result -1 */
    if (RDS8(s0 + 0x4Du) != -1) {
        return 0xFFFFFFFFu;
    }
    /* 800FD868..800FD874: byte+5 is incremented and the new value (not truncated) is returned */
    v = RD8(s0 + 5u) + 1u;
    WR8(s0 + 5u, v);
    return v;
}
