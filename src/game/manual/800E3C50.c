#include "manual.h"
#include "hex2c_rt.h"

/* 800E3C50 (PL00). $a0 = S (the actor struct), state byte at S+7 selects the case (jump table at 0x800DF0AC).
 * $a1, $a2, $a3 are passed on to some callees unchanged (cases 4 and 5, and the first call of cases 0, 1, 2, 3):
 * a call passes the entry value while no earlier call has run, and 0 after one (the callee clobbers the argument
 * registers, and the original never sets them there). The fall-through of the default is the lui in the delay slot. */
uint32_t man_800E3C50(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const uint32_t S = a0;

    switch (RD8(S + 7u)) {
    case 0: {
        uint32_t v5;

        WR16(S + 12u, 90u);
        WR16(S + 14u, 0u);
        WR8(S + 7u, RD8(S + 7u) + 1u);
        m->call4(m->ctx, 0x800E5CD0u, S, a1, a2, a3);
        v5 = (RD8(S + 11u) != 0u) ? 0x40000u : 0xFFFC0000u;
        WR32(S + 0x70u, v5);
        WR32(S + 0x7Cu, 0u);
        WR32(S + 0x74u, 0u);
        WR32(S + 0x78u, 0u);
        m->call4(m->ctx, 0x80024104u, S, 277u, 0u, 0u);
        m->call4(m->ctx, 0x800BB2A4u, S, 42u, 0u, 0u);
        return m->call4(m->ctx, 0x800268BCu, S, RD32(0x800EBC50u), 0u, 0u);
    }
    case 1: {
        uint32_t ca2 = a2;
        uint32_t ca3 = a3;

        if (RD8(S + 77u) != 128u) {
            uint32_t v;
            uint32_t v2;

            WR8(S + 7u, RD8(S + 7u) + 1u);
            v = m->call4(m->ctx, 0x80025548u, (RD8(S + 0x136u) + 4u) & 0xFFu, 0u, 4u, a3);
            if (v != 0u) {
                WR32(v + 0x80u, S);
                WR8(v + 4u, 0u);
            }
            v2 = m->call4(m->ctx, 0x80025548u, (RD8(S + 0x136u) + 4u) & 0xFFu, 0u, 4u, 0u);
            if (v2 != 0u) {
                WR32(v2 + 0x80u, S);
                WR8(v2 + 4u, 1u);
            }
            ca2 = 0u;
            ca3 = 0u;
        }
        return m->call4(m->ctx, 0x80026A04u, S, 0u, ca2, ca3);
    }
    case 2: {
        m->call4(m->ctx, 0x80029714u, S, 11u, 8u, a3);
        WR16(S + 12u, RD16(S + 12u) - 1u);
        if (RDS8(S + 12u) < 0) {
            WR8(S + 7u, RD8(S + 7u) + 1u);
            return m->call4(m->ctx, 0x800BB2A4u, S, 87u, 0u, 0u);
        }
        if (RDS8(S + 77u) == -1) {
            WR8(S + 77u, 0u);
            m->call4(m->ctx, 0x800BB2A4u, S,
                     RD8(0x800E7DC8u + (uint32_t)RDS8(S + 78u)), 0u, 0u);
        }
        m->call4(m->ctx, 0x800B428Cu, S, 0u, 0u, 0u);
        return m->call4(m->ctx, 0x80026A04u, S, 0u, 0u, 0u);
    }
    case 3: {
        WR16(S + 14u, 1u);
        WR8(S + 7u, RD8(S + 7u) + 1u);
        m->call4(m->ctx, 0x80024104u, S, 278u, a2, a3);
        return m->call4(m->ctx, 0x800268BCu, S, RD32(0x800EBC5Cu), 0u, 0u);
    }
    case 4:
        return m->call4(m->ctx, 0x800E5D1Cu, S, a1, a2, a3);
    case 5:
        return m->call4(m->ctx, 0x800E5D6Cu, S, a1, a2, a3);
    default:
        return 0x800E0000u;
    }
}
