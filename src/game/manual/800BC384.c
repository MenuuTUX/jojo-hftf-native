#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800AD2A4(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
uint32_t man_800BC51C(const GuestMem *m, uint32_t a0, uint32_t a1);

static uint32_t sext16(uint32_t x) { return (uint32_t)(int32_t)(int16_t)x; }

/* 800BC384 (AC_MAIN), frame 0x30. $a0 = A (struct), $a1 = 16-bit argument X. $a2 and $a3 are still the received values at the
 * first jal (8003D650 gets them unchanged), so nargs is 4.
 * The local at sp+0x10 is stored from the 8003D650 result (sw in the delay slot of jal 800AD2A4), its address is P for
 * 800AD2A4, and the code reads it back after that call, so the frame is reproduced in guest memory. */
uint32_t man_800BC384(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t s0, r, sp, arg0, arg1;

    s0 = RD32(a0 + 0x15Cu);
    if (RD8(0x80066CAEu) != 0u)
        return 1u;
    if (RD8(0x80066CA9u) != 0u)
        return 1u;

    if (RDS8(0x80066C6Cu) == 3 && RDS8(0x8006D2EBu) == RDS8(s0 + 0x136u)) {
        arg1 = 0u;
        arg0 = 0u;
    } else {
        arg1 = a1;
        arg0 = sext16(a1);
    }

    /* main path: lower the guest sp by the original frame before the first call */
    sp = m->sp_get(m->ctx) - 0x30u;
    m->sp_set(m->ctx, sp);

    r = m->call4(m->ctx, 0x8003D650u, arg0, arg1, a2, a3);
    WR32(sp + 0x10u, r);
    man_800AD2A4(m, a0, s0, sp + 0x10u, a3);

    r = m->call4(m->ctx, 0x8003D3C0u, RD32(sp + 0x10u), s0, sp + 0x10u, a3);
    WR16(s0 + 0x16Au, (uint16_t)(RD16(s0 + 0x16Au) - r));
    m->call4(m->ctx, 0x80029714u, a0, 14u, 8u, a3);
    m->call4(m->ctx, 0x80029714u, s0, 14u, 8u, a3);

    r = m->call4(m->ctx, 0x8003D3C0u, RD32(sp + 0x10u), 14u, 8u, a3);
    man_800BC51C(m, s0, sext16(r));

    if (RDS16(s0 + 0x16Au) >= 0) {
        m->sp_set(m->ctx, sp + 0x30u);
        return 0u;
    }
    if (RD8(a0 + 0x15Au) != 0u || RDS8(0x80066C6Cu) == 3) {
        WR16(s0 + 0x16Au, 0u);
        m->sp_set(m->ctx, sp + 0x30u);
        return 0u;
    }

    m->call4(m->ctx, 0x800BC608u, a0, 0x40u, 8u, a3);
    WR8(0x8006D2EEu, 1u);
    WR8(0x80066CA9u, 1u);
    WR8(s0 + 0x151u, 0u);
    r = RD8(a0 + 0x0Bu);
    WR8(s0 + 0x05u, 2u);
    WR8(s0 + 0x06u, 0u);
    WR8(s0 + 0x07u, 0u);
    WR8(s0 + 0x08u, 0u);
    WR8(s0 + 0x149u, 5u);
    WR8(s0 + 0x14Au, 5u);
    WR8(s0 + 0x163u, 0u);
    WR8(s0 + 0x106u, r);
    WR8(a0 + 0x162u, 0u);
    m->sp_set(m->ctx, sp + 0x30u);
    return 1u;
}
