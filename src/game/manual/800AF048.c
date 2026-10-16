#include "manual.h"
#include "hex2c_rt.h"

/* 800AF048 (AC_MAIN). $a0 = S (struct). Only $a0 is read.
 * Callees that take registers the original never sets (a1..a3, a0 after a call) get 0.
 * Indirect calls: jalr through the 2D table at 0x800CB554 (row = signed byte, col = signed byte). */
uint32_t man_800AF048(const GuestMem *m, uint32_t a0) {
    uint32_t s1 = a0;
    uint32_t s2;
    uint32_t v;
    uint32_t row;
    uint32_t col;
    uint32_t rp;
    uint32_t target;
    uint32_t lp;

    s2 = RD32(s1 + 0x170u);

    v = (RD8(0x80066CA3u) != 0u) ? 2u : 1u;
    WR8(s1 + 6u, v);
    WR8(s1 + 7u, 0u);
    WR8(s1 + 8u, 0u);
    WR8(s1 + 9u, 0u);
    WR8(s1 + 10u, 0u);
    WR8(s1 + 0x22Bu, 0u);
    m->call4(m->ctx, 0x800B4654u, s1, 0u, 0u, 0u);
    m->call4(m->ctx, 0x800B4648u, s1, 0u, 0u, 0u);

    if (RDS8(0x80066C6Cu) == 3) {
        m->call4(m->ctx, 0x800A1EFCu, 0u, 0u, 0u, 0u);
    }
    WR16(s2 + 0x16Au, 80u);
    WR16(s2 + 0x1E8u, 80u);
    WR8(s2 + 0x226u, 0u);
    WR8(s2 + 0x224u, 2u);
    WR8(s2 + 0x225u, 2u);

    row = (uint32_t)RDS8(s1 + 0x136u);
    col = (uint32_t)RDS8(s1 + 0x137u);
    rp = RD32(0x800CB554u + row * 4u);
    target = RD32(rp + col * 4u);
    m->calli(m->ctx, target, s1, 0u, 0u, 0u);

    row = (uint32_t)(RDS8(s2 + 0x136u) + 2);
    col = (uint32_t)RDS8(s2 + 0x137u);
    rp = RD32(0x800CB554u + row * 4u);
    target = RD32(rp + col * 4u);
    m->calli(m->ctx, target, s2, 0u, 0u, 0u);

    lp = RD32(RD32(s1 + 0x130u) + 0x10u);
    while (RD32(lp) != 0xFFFFFFFFu) {
        uint32_t key = RD32(lp);
        uint32_t val = RD32(lp + 4u);
        uint32_t idx = RD16(0x8005AA4Cu + ((uint32_t)(RDS8(s1 + 0x136u) + 4) << 1));
        WR32(0x8005AA8Cu + ((key + idx) << 2), val);
        lp += 8u;
    }

    lp = RD32(RD32(s1 + 0x130u) + 0x14u);
    while (RD32(lp) != 0xFFFFFFFFu) {
        uint32_t key = RD32(lp);
        uint32_t val = RD32(lp + 4u);
        uint32_t idx = RD16(0x8005AA4Cu + ((uint32_t)(RDS8(s1 + 0x136u) + 2) << 1));
        WR32(0x8005AA8Cu + ((key + idx) << 2), val);
        lp += 8u;
    }

    if (RD8(0x80066CA3u) != 0u) {
        m->call4(m->ctx, 0x800B4AD0u, s1, 0u, 0u, 0u);
    } else {
        target = RD32(RD32(s1 + 0x12Cu) + 0x40u);
        m->calli(m->ctx, target, s1, 0u, 0u, 0u);
    }
    m->call4(m->ctx, 0x800258F0u, s1, 1u, 0u, 0u);
    return m->call4(m->ctx, 0x800258F0u, s2, 1u, 0u, 0u);
}
