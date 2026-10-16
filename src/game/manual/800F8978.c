#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800F8978(const GuestMem *m, uint32_t a0) {
    uint32_t s0 = a0;
    uint32_t s1;
    uint32_t result;
    uint32_t v7;
    int32_t v3;

    /* 800F898C..800F8994: s1 = byte 7 */
    s1 = RD8(s0 + 7u);

    if (s1 == 0u) {
        /* 800F899C..800F89A4: a1 = 276; byte 7 += 1 */
        WR8(s0 + 7u, s1 + 1u);
        /* 800F89AC: halfword 0xC = 0 (delay slot of jal, runs before the call) */
        WR16(s0 + 12u, 0u);
        /* 800F89A8: jal 80024104 (a0 = s0, a1 = 276) */
        m->call4(m->ctx, 0x80024104u, s0, 276u, 0u, 0u);
        /* 800F89B0..800F89D0: halfword 0xE = signed byte at 0x800F4818[byte 0x207] */
        WR16(s0 + 14u, (uint32_t)RDS8(0x800F4818u + RD8(s0 + 0x207u)));
        /* 800F89D4..800F8A98: jal 800268BC with a1 = word 0x80102264 */
        return m->call4(m->ctx, 0x800268BCu, s0, RD32(0x80102264u), 0u, 0u);
    }

    if (s1 == 1u) {
        /* 800F89F0..800F8A00: halfword 0xE -= 1 (stored before the call), then jal 80026A04 */
        WR16(s0 + 14u, RD16(s0 + 14u) - 1u);
        m->call4(m->ctx, 0x80026A04u, s0, 0u, 0u, 0u);
        /* 800F8A04: result = signed byte 0x4E */
        result = (uint32_t)RDS8(s0 + 0x4Eu);
        if (result != 0u) {
            /* 800F8A10..800F8A1C: a1 = 0; byte 0x4E = 0 */
            WR8(s0 + 0x4Eu, 0u);
            /* 800F8A14..800F8A20: a0 = signed byte 0x136 + 2, a2 = byte 0x207, a1 = 0 */
            v7 = m->call4(m->ctx, 0x80025548u, (uint32_t)RDS8(s0 + 0x136u) + 2u, 0u,
                          (uint32_t)RD8(s0 + 0x207u), 0u);
            /* 800F8A28..800F8A34: if non-null, byte 4 = 1 and word 0x80 = s0 */
            if (v7 != 0u) {
                WR8(v7 + 4u, 1u);
                WR32(v7 + 0x80u, s0);
            }
            /* 800F8A38..800F8A48: byte 7 += 1; the returned $v0 is that new value */
            result = RD8(s0 + 7u) + 1u;
            WR8(s0 + 7u, result);
        }
        return result;
    }

    if (s1 == 2u) {
        uint32_t nv;
        uint32_t r;
        /* 800F8A54: jal 80026A04 (a0 = s0) */
        m->call4(m->ctx, 0x80026A04u, s0, 0u, 0u, 0u);
        /* 800F8A5C..800F8A6C: halfword 0xE -= 1; r = sign-extended low halfword */
        nv = RD16(s0 + 14u) - 1u;
        WR16(s0 + 14u, nv);
        r = (uint32_t)(int32_t)(int16_t)(nv & 0xFFFFu);
        if ((int32_t)r != -1) {
            return r;
        }
        /* 800F8A7C..800F8A88: byte 7 += 1; a1 = word 0x8010222C */
        WR8(s0 + 7u, RD8(s0 + 7u) + 1u);
        return m->call4(m->ctx, 0x800268BCu, s0, RD32(0x8010222Cu), 0u, 0u);
    }

    if (s1 == 3u) {
        /* 800F8AAC: jal 80026A04 (a0 = s0) */
        m->call4(m->ctx, 0x80026A04u, s0, 0u, 0u, 0u);
        /* 800F8AB4..800F8ABC: byte 0xB8 = 0x80; v3 = signed byte 0x4D */
        v3 = RDS8(s0 + 0x4Du);
        WR8(s0 + 0xB8u, 0x80u);
        if (v3 != -1) {
            return 0xFFFFFFFFu;
        }
        /* 800F8AD0..800F8AE0: byte 7 += 1 (before the call); a1 = 109 */
        WR8(s0 + 7u, RD8(s0 + 7u) + 1u);
        return m->call4(m->ctx, 0x80024104u, s0, 109u, 0u, 0u);
    }

    /* 800F8AEC..800F8AF0: default case, jal 800FB6EC (a0 = s0) */
    return m->call4(m->ctx, 0x800FB6ECu, s0, 0u, 0u, 0u);
}
