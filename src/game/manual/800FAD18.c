#include "manual.h"
#include "hex2c_rt.h"

/* 800FAD18 (PL02X): one object state machine, with its shared tail 800FAEA8 inlined. The original falls through
 * into 800FAEA8 (no frame of its own) and jumps to 800FAF5C and 800FAF94 inside it, so all of it is one function.
 * $s0 = the object ($a0), $s1 = the pointer at object+0x170 (read once on entry). State byte at object+7:
 * 0 = init, 1 = wait on 800C36F4, 2 = run (800B0160, 800B1094 pick sub-cases, then the timer at +0x0C and the
 * tail 800FAEA8), anything else = 800BB648 path. $a1..$a3 are passed to the first call of each path only (the
 * callee clobbers them, as in 80010354); every later call gets zero. Returns the $v0 left at the epilogue. */
uint32_t man_800FAD18(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t s0 = a0;
    uint32_t s1 = RD32(s0 + 0x170u);
    uint32_t v0, v1;

    v1 = RD8(s0 + 0x7u);
    if (v1 == 0u) {
        /* state 0 */
        m->call4(m->ctx, 0x80026A04u, s0, a1, a2, a3);
        v0 = RD8(s0 + 0x7u);
        WR8(s0 + 0x21Fu, 0u);
        v0 = v0 + 1u;
        WR8(s0 + 0x7u, v0);
        WR8(s1 + 0x21Fu, 0u);
        v1 = RD8(s0 + 0x262u);
        v0 = 3u;
        WR32(s0 + 0x7Cu, 0u);
        WR32(s0 + 0x74u, 0u);
        WR16(s0 + 0xCu, v0);
        if (v1 != 0u) {
            v1 = 0xFFFE8000u;
            v0 = 0xFFF80000u;
        } else {
            v1 = 0xFFFE8000u;
            v0 = 0x000A0000u;
        }
        WR32(s0 + 0x70u, v0);
        WR32(s0 + 0x78u, v1);
        v0 = RD8(s0 + 0xBu);
        if (v0 == 0u) {
            v0 = 0u - RD32(s0 + 0x70u);
            v1 = 0u - RD32(s0 + 0x78u);
            WR32(s0 + 0x70u, v0);
            WR32(s0 + 0x78u, v1);
        }
        return v0;
    }

    if (v1 == 1u) {
        /* state 1 */
        v0 = m->call4(m->ctx, 0x800C36F4u, s0, a1, a2, a3);
        if (v0 != 0u)
            return m->call4(m->ctx, 0x800C4354u, s0, 0, 0, 0);
        m->call4(m->ctx, 0x80026A04u, s0, 0, 0, 0);
        v0 = RD16(s0 + 0xCu) - 1u;
        WR16(s0 + 0xCu, v0);
        {
            uint32_t t = (uint32_t)((int32_t)(v0 << 16) >> 16);
            v0 = 10u;                       /* delay slot of the bne: runs on both paths */
            if (t != 0xFFFFFFFFu)
                return v0;
        }
        v0 = 10u;
        WR16(s0 + 0xCu, v0);
        v0 = RD8(s0 + 0x7u);
        v1 = 16u;
        WR16(s0 + 0xEu, v1);
        v0 = v0 + 1u;
        WR8(s0 + 0x7u, v0);
        return v0;
    }

    if (v1 != 2u) {
        /* states other than 0, 1, 2 (800FAF5C) */
        m->call4(m->ctx, 0x800BB648u, s0, a1, a2, a3);
        m->call4(m->ctx, 0x80026A04u, s0, 0, 0, 0);
        {
            uint32_t r = m->call4(m->ctx, 0x800B46A4u, s0, 0, 0, 0);
            v0 = 0xFFFFFFFFu;               /* delay slot of the bne: runs on both paths */
            if (r != 0u)
                return v0;
        }
        v1 = (uint32_t)RDS8(s0 + 0x4Du);
        if (v1 != v0)
            return v0;
        return m->call4(m->ctx, 0x800C1F38u, s0, 0, 0, 0);
    }

    /* state 2 */
    v0 = m->call4(m->ctx, 0x800C36F4u, s0, a1, a2, a3);
    if (v0 != 0u)
        return m->call4(m->ctx, 0x800C4354u, s0, 0, 0, 0);
    v0 = m->call4(m->ctx, 0x800B0160u, s0, 0, 0, 0);
    if (v0 != 0u)
        return m->call4(m->ctx, 0x800C2E80u, s0, 0, 0, 0);
    v0 = m->call4(m->ctx, 0x800B1094u, s0, 0, 0, 0);
    if (v0 != 0u)
        return m->call4(m->ctx, 0x800C2A3Cu, s0, 0, 0, 0);

    /* 800FAE84: tail shared with the 800FAEA8 continuation */
    v0 = RD32(s0 + 0x68u) + RD32(s0 + 0x70u);
    WR32(s0 + 0x68u, v0);
    v0 = RD16(s0 + 0xEu) - 1u;
    v1 = ((int32_t)RDS16(s0 + 0xCu) < 4) ? 1u : 0u;
    WR16(s0 + 0xEu, v0);                    /* delay slot of the bne below */
    if (v1 == 0u) {
        uint32_t t = (RD8(s0 + 0x262u) != 0u) ? (RD16(s0 + 0x1CCu) & 0x8000u)
                                              : (RD16(s0 + 0x1CCu) & 0x2000u);
        v0 = 3u;                            /* delay slot of the beq below: runs on both paths */
        if (t != 0u)
            WR16(s0 + 0xCu, v0);
    }
    v0 = RD16(s0 + 0xCu) - 1u;
    WR16(s0 + 0xCu, v0);
    if ((v0 << 16) != 0u)
        return m->call4(m->ctx, 0x80026A04u, s0, 0, 0, 0);

    /* 800FAF00: the counter reached zero, advance the state */
    v0 = RD8(s0 + 0x7u) + 1u;
    WR8(s0 + 0x7u, v0);
    v0 = 1u;
    WR8(s1 + 0x21Fu, v0);
    v0 = RD8(s0 + 0x262u);
    if (v0 != 0u)
        return m->call4(m->ctx, 0x800268BCu, s0, RD32(0x80102108u), 0, 0);
    return m->call4(m->ctx, 0x800268BCu, s0, RD32(0x80102104u), 0, 0);
}
