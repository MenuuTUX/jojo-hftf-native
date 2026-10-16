#include "manual.h"
#include "hex2c_rt.h"

/* 800E3E60 (PL00). $a0 = S. Dispatches on the state byte S+7 through the jump table at 0x800DF0C4 (RAM dump:
 * 800E3EA8, 800E3F68, 800E3FD8, 800E4100, 800E4154). State 5 or more returns 0. The tail 800E4174 calls 80026A04.
 * $a1..$a3 are the registers as the code leaves them; after a call they are assumed unchanged (not modelled). */
uint32_t man_800E3E60(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    uint32_t s0 = a0;
    uint32_t v1, r, idx;

    v1 = RD8(s0 + 7u);
    if (v1 >= 5u)
        return 0u;

    switch (v1) {
    case 0u:
        WR8(s0 + 7u, RD8(s0 + 7u) + 1u);
        WR16(s0 + 12u, 15u);
        WR16(s0 + 14u, 0u);
        WR16(s0 + 396u, 60u);

        r = RD32(s0 + 0x70u);
        if ((int32_t)r < 0)
            r += 15u;
        WR32(s0 + 0x70u, (uint32_t)((int32_t)r >> 4));

        v1 = RD32(s0 + 0x74u);
        WR32(s0 + 0x78u, 0u);
        if ((int32_t)v1 < 0)
            v1 += 7u;
        WR32(s0 + 0x74u, (uint32_t)((int32_t)v1 >> 3));

        a2 = RD32(s0 + 0x7Cu);
        if ((int32_t)a2 < 0)
            a2 += 15u;
        WR32(s0 + 0x7Cu, (uint32_t)((int32_t)a2 >> 4));

        a1 = 277u;
        m->call4(m->ctx, 0x80024104u, s0, a1, a2, a3);
        a1 = 53u;
        m->call4(m->ctx, 0x800BB2A4u, s0, a1, a2, a3);
        m->call4(m->ctx, 0x800BB30Cu, s0, a1, a2, a3);

        idx = RD8(s0 + 0x207u);
        a1 = RD32(0x800EBB54u + 4u * (uint32_t)(int32_t)RDS8(0x800E7DD0u + idx));
        return m->call4(m->ctx, 0x800268BCu, s0, a1, a2, a3);

    case 1u:
        a1 = 0u;
        if (RD8(s0 + 0x4Du) != 128u) {
            a2 = 4u;
            WR8(s0 + 7u, RD8(s0 + 7u) + 1u);
            r = m->call4(m->ctx, 0x80025548u, (RD8(s0 + 0x136u) + a2) & 0xFFu, a1, a2, a3);
            if (r != 0u) {
                WR32(r + 0x80u, s0);
                WR8(r + 4u, 0u);
            }
            r = m->call4(m->ctx, 0x80025548u, (RD8(s0 + 0x136u) + a2) & 0xFFu, a1, a2, a3);
            if (r != 0u) {
                WR32(r + 0x80u, s0);
                WR8(r + 4u, 1u);
            }
        }
        break;

    case 2u:
        m->call4(m->ctx, 0x800B428Cu, s0, a1, a2, a3);
        r = m->call4(m->ctx, 0x800B2738u, s0, a1, a2, a3);
        if (r != 0u) {
            a1 = 278u;
            m->call4(m->ctx, 0x80024104u, s0, a1, a2, a3);
            m->call4(m->ctx, 0x800244D4u, s0, a1, a2, a3);
            a1 = 16u;
            WR16(s0 + 0x6Eu, 35u);
            WR16(s0 + 0x0Eu, 1u);
            WR8(s0 + 7u, RD8(s0 + 7u) + 1u);
            return m->call4(m->ctx, 0x800B4AD0u, s0, a1, a2, a3);
        }
        WR16(s0 + 0x18Cu, RD16(s0 + 0x18Cu) - 1u);
        WR16(s0 + 0x0Cu, RD16(s0 + 0x0Cu) - 1u);
        if ((int32_t)RDS8(s0 + 0x0Cu) < 0 || RD16(s0 + 0x18Cu) == 0u) {
            a1 = 534u;
            m->call4(m->ctx, 0x80024104u, s0, a1, a2, a3);
            WR16(s0 + 0x0Eu, 1u);
            WR32(s0 + 0x74u, 0xFFFDC800u);
            WR32(s0 + 0x7Cu, 0xFFFF8000u);
            WR8(s0 + 7u, RD8(s0 + 7u) + 1u);
            a1 = 11u;
            m->call4(m->ctx, 0x800B4AD0u, s0, a1, a2, a3);
        } else {
            if ((RD16(s0 + 0x1D6u) & 0xB0u) != 0u)
                WR16(s0 + 0x0Cu, 15u);
        }
        if ((int32_t)RDS8(s0 + 0x4Du) == -1) {
            WR8(s0 + 0x4Du, 0u);
            a1 = RD8(0x800E7DCCu + (uint32_t)(int32_t)RDS8(s0 + 0x4Eu));
            m->call4(m->ctx, 0x800BB2A4u, s0, a1, a2, a3);
        }
        break;

    case 3u:
        m->call4(m->ctx, 0x800B428Cu, s0, a1, a2, a3);
        m->call4(m->ctx, 0x80026A04u, s0, a1, a2, a3);
        r = m->call4(m->ctx, 0x800B2738u, s0, a1, a2, a3);
        if (r == 0u)
            return 35u;
        WR16(s0 + 0x6Eu, 35u);
        WR16(s0 + 0x0Eu, 1u);
        WR8(s0 + 7u, RD8(s0 + 7u) + 1u);
        m->call4(m->ctx, 0x800244D4u, s0, a1, a2, a3);
        a1 = 16u;
        return m->call4(m->ctx, 0x800B4AD0u, s0, a1, a2, a3);

    case 4u:
        if ((int32_t)RDS8(s0 + 0x4Du) == -1)
            return m->call4(m->ctx, 0x800C1F38u, s0, a1, a2, a3);
        break;

    default:
        return 0u;
    }

    /* 800E4174: shared tail */
    return m->call4(m->ctx, 0x80026A04u, s0, a1, a2, a3);
}
