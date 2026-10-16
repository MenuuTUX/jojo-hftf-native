#include "manual.h"
#include "hex2c_rt.h"

/* 800A3268 (GAMESS): state step on the struct p (a0). g = signed byte 0x800A36E8, t = byte p[6].
 * g < 0: tail-calls 800A3378 with a0 = p and a1..a3 unchanged (they are passed on, hence nargs 4); its $v0 is returned.
 * t = 0: returns 1; writes p[6]=1 and p[0x88]=0 only when g == 0.
 * t = 1 or 3: calls 800A3434(p); if p[0x88] is 0 returns 0; else p[6]++ and 0x800A36E8++ and returns the new byte value
 *   (t=1 also clears p[0x88] first).
 * t = 2: returns 3 when g != 0; else p[6]=3, p[0x88]=0, p[4]^=1, and returns 800A33A8(p) with a1 = t.
 * other t: returns 3.
 * The calls to 800A3434 and 800A33A8 pass a1 = t (the byte the original loaded into $a1). */
uint32_t man_800A3268(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p, t, g8;
    int32_t g;

    p = a0;
    g = RDS8(0x800A36E8u);
    if (g < 0) {
        return m->call4(m->ctx, 0x800A3378u, p, a1, a2, a3);
    }

    t = RD8(p + 6u);
    if (t == 0u) {
        if (g == 0) {
            WR8(p + 6u, 1u);
            WR8(p + 0x88u, 0u);
        }
        return 1u;
    }

    if (t == 1u) {
        m->call4(m->ctx, 0x800A3434u, p, t, a2, a3);
        if (RDS8(p + 0x88u) == 0) {
            return 0u;
        }
        WR8(p + 0x88u, 0u);
        WR8(p + 6u, RD8(p + 6u) + 1u);
        g8 = RD8(0x800A36E8u);
        WR8(0x800A36E8u, g8 + 1u);
        return g8 + 1u;
    }

    if (t == 2u) {
        if (g != 0) {
            return 3u;
        }
        WR8(p + 6u, 3u);
        WR8(p + 0x88u, 0u);
        WR8(p + 4u, RD8(p + 4u) ^ 1u);
        return m->call4(m->ctx, 0x800A33A8u, p, t, a2, a3);
    }

    if (t == 3u) {
        m->call4(m->ctx, 0x800A3434u, p, t, a2, a3);
        if (RDS8(p + 0x88u) == 0) {
            return 0u;
        }
        WR8(p + 6u, RD8(p + 6u) + 1u);
        g8 = RD8(0x800A36E8u);
        WR8(0x800A36E8u, g8 + 1u);
        return g8 + 1u;
    }

    return 3u;
}
