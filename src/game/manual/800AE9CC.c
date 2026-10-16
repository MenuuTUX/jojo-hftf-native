#include "manual.h"
#include "hex2c_rt.h"

/* 800AE9CC (AC_MAIN): writes the 8 mask bits of a1 into the 8-byte slots at 0x800CD250 + 8*A[2] (slot byte i = a1 & mask[i],
 * masks 80,10,20,40,04,01,08,02), then ORs 16-bit lookups for i = 0..7 into a3. The lookup source is the 18-byte table copied
 * from 0x800A6400 into the guest stack frame (sp-0x18), so out-of-range index reads hit the same memory as the original.
 * A[2] is read once before the loop (the slot stores cannot change it: they stay below A+2). Stores (a1 & 0xF900) | a3 to A+0x1D8,
 * and returns that value in $v0. */
uint32_t man_800AE9CC(const GuestMem *m, uint32_t a0, uint32_t a1) {
    uint32_t sp, a3, i, k, slot, idx, v0;
    uint32_t masks[8];
    int32_t mode;

    sp = m->sp_get(m->ctx) - 0x18u;
    m->sp_set(m->ctx, sp);
    for (i = 0u; i < 18u; i++) {
        WR8(sp + i, RD8(0x800A6400u + i));
    }

    masks[0] = 0x80u;
    masks[1] = 0x10u;
    masks[2] = 0x20u;
    masks[3] = 0x40u;
    masks[4] = 0x04u;
    masks[5] = 0x01u;
    masks[6] = 0x08u;
    masks[7] = 0x02u;
    for (i = 0u; i < 8u; i++) {
        WR8(0x800CD250u + 8u * RD8(a0 + 2u) + i, a1 & masks[i]);
    }

    a3 = 0u;
    k = RD8(a0 + 2u);
    for (i = 0u; i < 8u; i++) {
        slot = RD8(0x800CD250u + 8u * k + i);
        if (slot != 0u) {
            mode = RDS8(0x80066C6Cu);
            if ((mode == 0 || mode == 5) && RDS8(0x80066CC4u) != 0) {
                a3 |= RD16(sp + 2u * i);
            } else {
                idx = RD16(0x8006CE30u + 16u * k + 2u * i);
                a3 |= RD16(sp + 2u * idx);
            }
        }
    }

    v0 = (a1 & 0xFFFFF900u) | a3;
    WR16(a0 + 0x1D8u, v0);
    m->sp_set(m->ctx, sp + 0x18u);
    return v0;
}
