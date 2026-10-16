#include "manual.h"
#include "hex2c_rt.h"

/* 800E49A0: state machine on the byte at a0+7 (0..5, anything else returns 0x800E0000).
 * $a1 is loaded from a0+0x17C at entry, so it is not an input; $a2 and $a3 pass through
 * to callees (nargs = 4). $a1 and $a2 are tracked as the MIPS registers would hold them:
 * the callee is assumed to leave the argument registers it does not read unchanged. */
static uint32_t label11(const GuestMem *m, uint32_t s0, uint32_t a1v, uint32_t a2v, uint32_t a3v) {
    m->call4(m->ctx, 0x800BB648u, s0, a1v, a2v, a3v);
    return m->call4(m->ctx, 0x80026A04u, s0, a1v, a2v, a3v);
}

uint32_t man_800E49A0(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t s0 = a0;
    uint32_t s1 = RD32(s0 + 0x170u);   /* lw s1,0x170(a0), callee-saved, stays valid across calls */
    uint32_t a1v = RD32(s0 + 0x17Cu);  /* lw a1,0x17C(a0) at entry; the original overwrites a1 here */
    uint32_t a2v = a2;
    uint32_t a3v = a3;
    uint32_t st = RD8(s0 + 7u);
    uint32_t i;
    uint32_t v11;
    uint32_t v12;
    uint32_t v17;
    uint32_t v18;
    uint32_t v20;
    uint32_t r;
    int32_t v9;
    uint32_t p;

    (void)a1;
    if (st >= 6u) {
        return 0x800E0000u;
    }

    switch (st) {
    case 0u:
        WR8(s0 + 7u, st + 1u);
        m->call4(m->ctx, 0x800E5CD0u, s0, a1v, a2v, a3v);
        WR16(s0 + 12u, (uint32_t)(int32_t)RDS8(0x800E7E00u + RD8(s0 + 0x207u)));
        a1v = RD8(0x800E7DFCu + RD8(s0 + 0x207u));
        m->call4(m->ctx, 0x800BB2A4u, s0, a1v, a2v, a3v);
        m->call4(m->ctx, 0x800BB30Cu, s0, a1v, a2v, a3v);
        v9 = RDS8(0x800E7E04u + RD8(s0 + 0x207u));
        p = 0x800EBB54u + ((uint32_t)v9 << 2);
        a1v = RD32(p);
        return m->call4(m->ctx, 0x800268BCu, s0, a1v, a2v, a3v);

    case 1u:
        WR8(s0 + 7u, st + 1u);
        WR8(s1 + 0x21Fu, 1u);
        i = RD8(s0 + 0x207u);
        WR32(s0 + 0x70u, RD32(0x800E7B80u + 12u * i));
        v11 = RD32(0x800E7B80u + 12u * i + 4u);
        v12 = RD8(s0 + 11u);
        WR32(s0 + 0x7Cu, 0u);
        WR32(s0 + 0x74u, 0u);
        WR32(s0 + 0x78u, v11);
        if (v12 != 0u) {
            WR32(s0 + 0x70u, 0u - RD32(s0 + 0x70u));
            WR32(s0 + 0x78u, 0u - RD32(s0 + 0x78u));
        }
        return m->call4(m->ctx, 0x80026A04u, s0, a1v, a2v, a3v);

    case 2u:
        WR16(s0 + 0x0Cu, RD16(s0 + 0x0Cu) - 1u);
        if (RDS8(s0 + 0x0Cu) >= 0) {
            /* jal 800E5CF4 with $a2 = 0x50 set in its delay slot */
            a2v = 0x50u;
            if (m->call4(m->ctx, 0x800E5CF4u, s0, a1v, a2v, a3v) == 0u) {
                return label11(m, s0, a1v, a2v, a3v);
            }
        }
        WR8(s0 + 7u, RD8(s0 + 7u) + 1u);
        i = RD8(s0 + 0x207u) + 3u;
        WR32(s0 + 0x70u, RD32(0x800E7B80u + 12u * i));
        v17 = RD32(0x800E7B80u + 12u * i + 4u);
        v18 = RD8(s0 + 11u);
        WR32(s0 + 0x78u, v17);
        if (v18 != 0u) {
            WR32(s0 + 0x70u, 0u - RD32(s0 + 0x70u));
            WR32(s0 + 0x78u, 0u - RD32(s0 + 0x78u));
        }
        v9 = RDS8(0x800E7E04u + i);
        p = 0x800EBB54u + ((uint32_t)v9 << 2);
        a1v = RD32(p);
        return m->call4(m->ctx, 0x800268BCu, s0, a1v, a2v, a3v);

    case 3u:
        if (RD8(s0 + 0x4Du) == 128u) {
            WR8(s0 + 0x4Du, 0u);
            m->call4(m->ctx, 0x80024104u, s0, 274u, a2v, a3v);
            a1v = 0u;
            a2v = 1u;
            v20 = m->call4(m->ctx, 0x80025548u, (RD8(s0 + 0x136u) + 4u) & 0xFFu, a1v, a2v, a3v);
            if (v20 != 0u) {
                WR32(v20 + 0x80u, s0);
            }
        }
        if (RDS8(s0 + 0x4Du) == -1) {
            r = RD8(s0 + 7u);
            WR8(s0 + 0x4Du, 0u);
            WR8(s0 + 7u, r + 1u);
            WR8(s1 + 0x21Fu, 1u);
        }
        return label11(m, s0, a1v, a2v, a3v);

    case 4u:
        if (RDS8(s0 + 0x4Du) == -1) {
            r = RD8(s0 + 7u);
            WR8(s0 + 0x4Du, 0u);
            WR8(s0 + 7u, r + 1u);
            WR8(s1 + 0x21Fu, 1u);
        }
        return m->call4(m->ctx, 0x80026A04u, s0, a1v, a2v, a3v);

    case 5u:
        r = m->call4(m->ctx, 0x800B46A4u, s0, a1v, a2v, a3v);
        if (r != 0u) {
            return 0xFFFFFFFFu;
        }
        if (RDS8(s0 + 0x4Du) == -1) {
            return m->call4(m->ctx, 0x800C1F38u, s0, a1v, a2v, a3v);
        }
        return m->call4(m->ctx, 0x80026A04u, s0, a1v, a2v, a3v);

    default:
        return 0x800E0000u;
    }
}
