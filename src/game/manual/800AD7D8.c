#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800AD994(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2);
uint32_t man_800ADA94(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2);
uint32_t man_800BC51C(const GuestMem *m, uint32_t a0, uint32_t a1);

static uint32_t sext16(uint32_t x) { return (uint32_t)(int32_t)(int16_t)x; }

/* 800AD7D8 (AC_MAIN), frame 0x38. $a0 = A (struct), $a1 = B (struct). $a2 and $a3 are still the received values at every
 * jal of the main path (8003D650, 8003D020, 8003D3C0 get them unchanged), so nargs is 4.
 * Locals at sp+0x10 and sp+0x14 are stored by the code and their addresses are passed to 800AD994 and 800ADA94 (P), which
 * write through them, so the frame is reproduced in guest memory and the locals are read back with RD32 after those calls.
 * The 8003D650 calls take $a1 = the received value, or 0x43800000 after the lui in the negative branch. */
uint32_t man_800AD7D8(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t s1, s0v, d, a1r, sp, s4, s5, v, r;
    int32_t b;

    s4 = RD32(a1 + 0x174u);
    s5 = RD32(a1 + 0x178u);

    /* early exits: the value returned is the $v0 that the original holds at the exit */
    if (RDS8(0x80066C6Cu) == 3) {
        int32_t v136 = RDS8(a1 + 0x136u);
        if (RDS8(0x8006D2EBu) == v136)
            return (uint32_t)v136;
    }
    v = RD8(0x80066CAEu);
    if (v != 0u)
        return v;

    s1 = RD32(RD32(a0 + 0x12Cu) + 0x18u) + 48u * RD8(a0 + 0x138u);
    v = RD8(s1 + 0x1Du);
    if (v != 0u)
        return v;

    /* main path: lower the guest sp by the original frame before the first call */
    sp = m->sp_get(m->ctx) - 0x38u;
    m->sp_set(m->ctx, sp);

    s0v = (uint32_t)(int32_t)RDS8(s1 + 0u);
    d = m->call4(m->ctx, 0x8003D650u, s0v, a1, a2, a3);
    WR32(sp + 0x10u, d);
    a1r = a1;
    if ((int32_t)s0v < 0) {
        a1r = 0x43800000u;
        d = m->call4(m->ctx, 0x8003D020u, d, 0x43800000u, a2, a3);
        WR32(sp + 0x10u, d);
    }

    s0v = (uint32_t)(int32_t)RDS8(s1 + 1u);
    d = m->call4(m->ctx, 0x8003D650u, s0v, a1r, a2, a3);
    WR32(sp + 0x14u, d);
    if ((int32_t)s0v < 0) {
        d = m->call4(m->ctx, 0x8003D020u, d, 0x43800000u, a2, a3);
        WR32(sp + 0x14u, d);
    }

    man_800AD994(m, a0, a1, sp + 0x10u);
    man_800ADA94(m, a0, a1, sp + 0x14u);

    d = m->call4(m->ctx, 0x8003D3C0u, RD32(sp + 0x10u), a1, sp + 0x14u, a3);
    WR16(s4 + 0x16Au, (uint16_t)(RD16(s4 + 0x16Au) - d));

    b = RDS8(a0 + 0x136u);
    v = RD8(0x800CDD50u + 912u * (uint32_t)b + 0x203u);
    if (v == 0u) {
        d = m->call4(m->ctx, 0x8003D3C0u, RD32(sp + 0x14u), a1, sp + 0x14u, a3);
        WR16(s5 + 0x16Au, (uint16_t)(RD16(s5 + 0x16Au) - d));
    }

    if (RDS16(a1 + 0x16Au) < 0)
        WR16(a1 + 0x16Au, 0u);

    d = m->call4(m->ctx, 0x8003D3C0u, RD32(sp + 0x10u), a1, sp + 0x14u, a3);
    r = man_800BC51C(m, a1, sext16(d));
    m->sp_set(m->ctx, sp + 0x38u);
    return r;
}
