#include "manual.h"
#include "hex2c_rt.h"

/* 800A1820: never returns. Clears the state bytes at 0x80066C48, calls 80024DC4 (a0..a3 pass on, unwritten), 800A1FCC,
 * 80027FA8, 8001EE6C, 8002CAB0, 8002D138, 80010914(1), sets 0x80066C75 = 7, then loops: increment the byte at
 * 0x80066C60, 800A1CFC(0x80066C48), 800A18CC(0x80066C48, 0x8006D2D0), 800A1CFC(0x80066C48), 80010914(1). */
uint32_t man_800A1820(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const uint32_t s0 = 0x80066C48u;
    const uint32_t s1 = 0x8006D2D0u;
    uint32_t v;

    WR8(s0 + 0x1Du, 0);
    WR8(s0 + 0x1Eu, 0);
    WR8(s0 + 0x1Fu, 0);
    WR16(s0, 0);
    WR16(s0 + 2u, 0);
    m->call4(m->ctx, 0x80024DC4u, a0, a1, a2, a3);
    m->call4(m->ctx, 0x800A1FCCu, 0u, 0u, 0u, 0u);
    m->call4(m->ctx, 0x80027FA8u, 0u, 0u, 0u, 0u);
    m->call4(m->ctx, 0x8001EE6Cu, 0u, 0u, 0u, 0u);
    m->call4(m->ctx, 0x8002CAB0u, 0u, 0u, 0u, 0u);
    m->call4(m->ctx, 0x8002D138u, 0u, 0u, 0u, 0u);
    m->call4(m->ctx, 0x80010914u, 1u, 0u, 0u, 0u);
    WR8(s0 + 0x2Du, 7);
    WR8(s0 + 0x2Eu, 0);
    for (;;) {
        v = RD8(s0 + 0x18u) + 1u;
        WR8(s0 + 0x18u, v);
        m->call4(m->ctx, 0x800A1CFCu, s0, 0u, 0u, 0u);
        m->call4(m->ctx, 0x800A18CCu, s0, s1, 0u, 0u);
        m->call4(m->ctx, 0x800A1CFCu, s0, 0u, 0u, 0u);
        m->call4(m->ctx, 0x80010914u, 1u, 0u, 0u, 0u);
    }
}
