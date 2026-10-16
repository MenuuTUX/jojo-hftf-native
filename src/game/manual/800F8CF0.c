#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800F8CF0(const GuestMem *m, uint32_t a0) {
    uint32_t s0 = a0;
    uint32_t s1;
    uint32_t v;
    uint32_t d;
    int32_t v5;

    /* 800F8D04: state byte +7 == 0 is the first-entry path: set it to 1, init, then tail-call 800268BC */
    if (RD8(s0 + 7u) == 0u) {
        WR8(s0 + 7u, 1u);
        m->call4(m->ctx, 0x80024104u, s0, 276u, 0u, 0u);
        WR16(s0 + 12u, 0u);
        WR16(s0 + 14u, (uint32_t)RDS8(0x800F4818u + RD8(s0 + 0x207u)));
        return m->call4(m->ctx, 0x800268BCu, s0, RD32(0x801021BCu), 0u, 0u);
    }

    m->call4(m->ctx, 0x80026A04u, s0, 0u, 0u, 0u);
    s1 = RD8(s0 + 7u);

    if (s1 == 1u) {
        /* 800F8D74..800F8DE0: halfword +14 decremented (delay slot stores it); return it when byte +78 is 0 */
        v5 = RDS8(s0 + 78u);
        d = (uint32_t)RD16(s0 + 14u) - 1u;
        WR16(s0 + 14u, d);
        if (v5 == 0) {
            return d;
        }
        WR8(s0 + 0x4eu, 0u);
        WR16(s0 + 12u, 4u);
        v = m->call4(m->ctx, 0x80025548u, (uint32_t)RDS8(s0 + 0x136u) + 2u, 0u,
                     (RD8(s0 + 0x207u) + 3u) & 0xffu, 0u);
        if (v != 0u) {
            WR8(v + 4u, s1);
            WR32(v + 0x80u, s0);
        }
    } else if (s1 == 2u) {
        /* 800F8DCC..800F8DF4: sign-extended halfword +14 decremented; when it reaches -1, call 800268BC */
        d = (uint32_t)RD16(s0 + 14u) - 1u;
        WR16(s0 + 14u, d);
        if ((int32_t)(int16_t)(uint16_t)d != -1) {
            return 0x80100000u; /* delay slot 800F8DE8 (lui v0,0x8010) runs on the taken bne at 800F8DE4 */
        }
        m->call4(m->ctx, 0x800268BCu, s0, RD32(0x801021C0u), 0u, 0u);
    } else {
        /* 800F8E0C: byte +77 signed == -1 tail-calls 800C1F38, otherwise return -1 */
        if (RDS8(s0 + 0x4du) == -1) {
            return m->call4(m->ctx, 0x800C1F38u, s0, 0u, 0u, 0u);
        }
        return 0xFFFFFFFFu;
    }

    /* 800F8DF8: state byte +7 incremented and returned */
    v = RD8(s0 + 7u) + 1u;
    WR8(s0 + 7u, v);
    return v;
}
