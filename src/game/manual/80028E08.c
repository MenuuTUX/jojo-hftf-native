#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800121F0(const GuestMem *m, uint32_t a0, uint32_t a1);

/* 80028E08 (TEXT). $a0 = option index (only its low byte is read). The original frame is 48 bytes; its local records are
 * at sp+0x10..sp+0x17 and each one is passed to man_800121F0 as $a0. $a1..$a3 are never written here, so the first call
 * (80028D9C) and every other call pass them on unchanged. The case index 0 is the table walk; 1 and 2 are the two fixed
 * menus; index 2 or more other than 2 returns 2 at once. */

static uint32_t body_80028E08(const GuestMem *m, uint32_t sp, uint32_t s0) {
    uint32_t buf = sp + 0x10u;
    uint32_t tb, ep, s1, idx, result, a1v, v;

    if (s0 == 1u) {
        /* 80028F90: "quit" box */
        WR16(buf + 0u, 152u);
        WR16(buf + 2u, 122u);
        WR16(buf + 4u, 3u);
        WR8(buf + 6u, 0u);
        WR8(buf + 7u, 0u);
        man_800121F0(m, buf, 0x8005A964u);
        v = RD8(RD32(0x8005A930u) + 8u);
        WR16(buf + 0u, 180u);
        WR16(buf + 2u, 108u);
        WR16(buf + 4u, (v != 0u) ? 9u : 3u);
        WR8(buf + 6u, 0u);
        WR8(buf + 7u, 0u);
        man_800121F0(m, buf, 0x80054998u);
        v = RD8(RD32(0x8005A930u) + 8u);
        WR16(buf + 0u, 184u);
        WR16(buf + 2u, 94u);
        WR16(buf + 4u, (v != 0u) ? 3u : 9u);
        WR8(buf + 6u, 0u);
        WR8(buf + 7u, 0u);
        return man_800121F0(m, buf, 0x8005499Cu);
    }

    if (s0 >= 2u) {
        if (s0 != 2u)
            return 2u;
        /* 80029078: "vibrate" box */
        WR16(buf + 0u, 160u);
        WR16(buf + 2u, 122u);
        WR16(buf + 4u, 3u);
        WR8(buf + 6u, 0u);
        WR8(buf + 7u, 0u);
        man_800121F0(m, buf, 0x8005A970u);
        v = RD8(RD32(0x8005A930u) + 8u);
        WR16(buf + 0u, 184u);
        WR16(buf + 2u, 108u);
        WR16(buf + 4u, (v != 0u) ? 9u : 3u);
        WR8(buf + 6u, 0u);
        WR8(buf + 7u, 0u);
        man_800121F0(m, buf, 0x800549A0u);
        v = RD8(RD32(0x8005A930u) + 8u);
        WR16(buf + 0u, 180u);
        WR16(buf + 2u, 94u);
        WR16(buf + 4u, (v != 0u) ? 3u : 9u);
        WR8(buf + 6u, 0u);
        WR8(buf + 7u, 0u);
        return man_800121F0(m, buf, 0x800549A4u);
    }

    /* s0 == 0, 80028E78: table walk over the entries of the current menu */
    idx = RD8(0x8008EBC0u);
    if (idx == 1u) {
        tb = (RDS8(0x80066C6Cu) == 5) ? 0x8005A9E8u : 0x8005A9A0u;
    } else if (idx == 2u) {
        tb = 0x8005AA0Cu;
    } else {
        tb = (RDS8(0x80066C6Cu) == 5) ? 0x8005A9C4u : 0x8005A97Cu;
    }

    /* 80028EE0: count = (signed byte at 0x8005A934 + index) + 1 */
    result = (uint32_t)((int32_t)RDS8(0x8005A934u + idx) + 1);
    if ((int32_t)result <= 0)
        return result;

    s1 = 0u;
    ep = tb + 8u;
    do {
        /* 80028F10..80028F5C */
        a1v = ((s1 & 0xFFu) == RD8(RD32(0x8005A930u) + 8u)) ? 3u : 9u;
        WR16(buf + 0u, RD16(ep - 2u));
        WR16(buf + 2u, RD16(ep));
        s1 += 1u;
        WR16(buf + 4u, a1v);
        WR8(buf + 6u, 0u);
        WR8(buf + 7u, 0u);
        man_800121F0(m, buf, RD32(tb));
        ep += 12u;
        /* 80028F60..80028F84 */
        result = (uint32_t)((int32_t)RDS8(0x8005A934u + RD8(0x8008EBC0u)) + 1);
        tb += 12u;
    } while ((int32_t)(s1 & 0xFFu) < (int32_t)result);
    return result;
}

uint32_t man_80028E08(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx) - 0x30u;   /* the original "addiu sp,sp,-48" */
    uint32_t r;

    m->sp_set(m->ctx, sp);
    WR8(0x8008EBC2u, 1u);                      /* 80028E1C */
    m->call4(m->ctx, 0x80028D9Cu, a0, a1, a2, a3);
    r = body_80028E08(m, sp, a0 & 0xFFu);
    m->sp_set(m->ctx, sp + 0x30u);
    return r;
}
