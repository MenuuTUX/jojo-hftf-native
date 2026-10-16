#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800103D0(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
uint32_t man_80025E24(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);

/* Screen draw with a per-entry table (segment TEXT). a0 selects a 16-bit entry (low byte), a1 selects the second table
 * entry for the 8..22 range (not 21). The original frame is 0x58 bytes; the SPRT p lives at sp+0x18 (s2 in the MIPS).
 * Returns the $v0 left at jr ra: 0. */
uint32_t man_8001DFDC(const GuestMem *m, uint32_t a0, uint32_t a1) {
    uint32_t sp = m->sp_get(m->ctx) - 0x58u;   /* 0x58 = the original "addiu sp,sp,-88" */
    uint32_t p = sp + 0x18u;                   /* SPRT p, $s2 in the original */
    uint32_t s0, s1, s3, outer = 0u, ent, tp, clut, result;

    m->sp_set(m->ctx, sp);

    if (RD8(0x80066C6Eu) == 0u) {
        WR8(0x80066C6Eu, 1u);
        man_800103D0(m, 0u, 0u, 0u, 0u);
        m->call4(m->ctx, 0x8003609Cu, 0u, 0u, 0u, 0u);      /* DrawSync(0) */
        WR8(0x801FC09Cu, 0u);
        WR8(0x801FBC2Cu, 0u);
        m->call4(m->ctx, 0x80010914u, 1u, 0u, 0u, 0u);
    }

    s0 = a0 & 0xFFu;
    m->call4(m->ctx, 0x8001ED60u, RD16(0x80059C60u + 2u * s0), 0u, 0u, 0u);
    man_80025E24(m, 0x80119800u, 0u, 0u, 0u);

    if (s0 >= 8u && s0 != 21u && s0 < 0x17u) {
        m->call4(m->ctx, 0x8001ED60u, RD16(0x80059C98u + 2u * (a1 & 0xFFu)), 0u, 0u, 0u);
        man_80025E24(m, 0x80119800u, 0u, 0u, 0u);
    }

    do {
        m->call4(m->ctx, 0x8003C210u, 0u, 0u, 0u, 0u);  /* VSync(0) */
        s1 = 0u;
        s3 = 0u;
        do {
            ent = 0x80059D50u + 12u * s1;
            tp = m->call4(m->ctx, 0x800397F0u, 1u, 0u,
                          (uint32_t)RDS16(ent + 8u), (uint32_t)RDS16(ent + 10u));
            m->call4(m->ctx, 0x80039980u, p, 0u, 1u, (uint32_t)(uint16_t)tp);
            m->call4(m->ctx, 0x80036634u, p, 0u, 0u, 0u);
            m->call4(m->ctx, 0x80039920u, p, 0u, 0u, 0u);
            clut = m->call4(m->ctx, 0x80039830u, 0u, 480u, 0u, 0u);
            WR16(p + 0x0Eu, clut);
            WR8(p + 0x04u, 0x80u);
            WR8(p + 0x05u, 0x80u);
            WR8(p + 0x06u, 0x80u);
            WR8(p + 0x0Cu, 0u);
            WR8(p + 0x0Du, 0u);
            WR16(p + 0x08u, RD16(ent + 0u));
            WR16(p + 0x0Au, RD16(ent + 2u));
            WR16(p + 0x10u, RD16(ent + 4u));
            WR16(p + 0x12u, RD16(ent + 6u));
            m->call4(m->ctx, 0x80036634u, p, 0u, 0u, 0u);
            s3 += 1u;
            s1 = (s1 & 3u) + 1u;
        } while (s3 < 4u);

        s0 = a0 & 0xFFu;
        if (s0 >= 8u && s0 != 21u && s0 < 0x17u) {
            tp = m->call4(m->ctx, 0x800397F0u, 1u, 0u, 576u, 0u);
            m->call4(m->ctx, 0x80039980u, p, 0u, 1u, (uint32_t)(uint16_t)tp);
            m->call4(m->ctx, 0x80036634u, p, 0u, 0u, 0u);
            m->call4(m->ctx, 0x80039920u, p, 0u, 0u, 0u);
            clut = m->call4(m->ctx, 0x80039830u, 0u, 481u, 0u, 0u);
            WR16(p + 0x0Eu, clut);
            ent = 0x80059D80u + 4u * (s0 - 8u);
            WR8(p + 0x04u, 0x80u);
            WR8(p + 0x05u, 0x80u);
            WR8(p + 0x06u, 0x80u);
            WR8(p + 0x0Cu, 0u);
            WR8(p + 0x0Du, 0u);
            WR16(p + 0x08u, RD16(ent + 0u));
            WR16(p + 0x0Au, RD16(ent + 2u));
            WR16(p + 0x10u, 152u);
            WR16(p + 0x12u, 96u);
            m->call4(m->ctx, 0x80036634u, p, 0u, 0u, 0u);
        }

        outer += 1u;
        result = (outer < 2u) ? 1u : 0u;
    } while (result != 0u);

    m->sp_set(m->ctx, sp + 0x58u);
    return result;
}
