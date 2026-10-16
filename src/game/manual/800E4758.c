#include "manual.h"
#include "hex2c_rt.h"

/* 800E4758 (PL00). $a0 = S (struct). Only $a0 is read. Switch on the byte at S+7 (jump table at 0x800DF124, cases 0..4).
 * Callee arguments the original does not set are passed as 0. The second 80025548 call in case 1 gets $a1 = 0 (not modelled). */

void man_800E4758(const GuestMem *m, uint32_t a0) {
    uint32_t s0 = a0;
    uint32_t s1 = s0 + 396u;
    uint32_t st = RD8(s0 + 7u);
    uint32_t v;
    uint32_t a0v;
    uint32_t ad;

    if (st == 0u) {
        WR8(s0 + 7u, st + 1u);
        m->call4(m->ctx, 0x800E5CD0u, s0, 0u, 0u, 0u);
        WR8(s0 + 0x107u, 60u);
        WR16(s0 + 12u, 60u);
        WR16(s0 + 14u, 0u);
        WR8(s0 + 0x21Fu, 0u);
        WR16(s1, 3u);
        v = (RD8(s0 + 11u) != 0u) ? 0x40000u : 0xFFFC0000u;
        WR32(s0 + 0x70u, v);
        WR32(s0 + 0x7Cu, 0u);
        WR32(s0 + 0x74u, 0u);
        WR32(s0 + 0x78u, 0u);
        m->call4(m->ctx, 0x800BB2A4u, s0, 75u, 0u, 0u);
        m->call4(m->ctx, 0x800268BCu, s0, RD32(0x800EBBF8u), 0u, 0u);
    } else if (st == 1u) {
        if (RD8(s0 + 0x21Fu) != 0u) {
            a0v = (RD8(s0 + 0x136u) + 4u) & 0xFFu;
            WR8(s0 + 7u, st + 1u);
            v = m->call4(m->ctx, 0x80025548u, a0v, 0u, 4u, 0u);
            if (v != 0u) {
                WR32(v + 0x80u, s0);
                WR8(v + 4u, 0u);
            }
            a0v = (RD8(s0 + 0x136u) + 4u) & 0xFFu;
            v = m->call4(m->ctx, 0x80025548u, a0v, 0u, 4u, 0u);
            if (v != 0u) {
                WR32(v + 0x80u, s0);
                WR8(v + 4u, 1u);
            }
            m->call4(m->ctx, 0x80024104u, s0, 277u, 0u, 0u);
            m->call4(m->ctx, 0x800268BCu, s0, RD32(0x800EBBFCu), 0u, 0u);
        }
        m->call4(m->ctx, 0x80026A04u, s0, 0u, 0u, 0u);
    } else if (st == 2u) {
        m->call4(m->ctx, 0x80029714u, s0, 0xBu, 8u, 0u);
        WR16(s0 + 12u, (uint32_t)(RD16(s0 + 12u) - 1u));
        if (RDS8(s0 + 12u) != 0) {
            if (RDS8(s0 + 0x4Du) == -1) {
                ad = 0x800E7DF8u + (uint32_t)(int32_t)RDS8(s0 + 0x4Eu);
                WR8(s0 + 0x4Du, 0u);
                m->call4(m->ctx, 0x800BB2A4u, s0, RD8(ad), 0u, 0u);
            }
            m->call4(m->ctx, 0x800B428Cu, s0, 0u, 0u, 0u);
            m->call4(m->ctx, 0x80026A04u, s0, 0u, 0u, 0u);
        } else {
            WR8(s0 + 7u, RD8(s0 + 7u) + 1u);
            m->call4(m->ctx, 0x800BB2A4u, s0, 89u, 0u, 0u);
        }
    } else if (st == 3u) {
        WR16(s0 + 14u, 1u);
        WR8(s0 + 0x21Fu, 1u);
        WR8(s0 + 7u, st + 1u);
        m->call4(m->ctx, 0x80024104u, s0, 278u, 0u, 0u);
        m->call4(m->ctx, 0x800268BCu, s0, RD32(0x800EBBD8u), 0u, 0u);
    } else if (st == 4u) {
        if (RDS8(s0 + 0x4Du) == -1) {
            m->call4(m->ctx, 0x800C1F38u, s0, 0u, 0u, 0u);
        }
        m->call4(m->ctx, 0x80026A04u, s0, 0u, 0u, 0u);
    }
}
