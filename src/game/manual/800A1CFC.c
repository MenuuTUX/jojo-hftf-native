#include "manual.h"
#include "hex2c_rt.h"

/* 800A1CFC falls through into 800A1D68 and branches into the same tail (800A1D88..800A1F18).
   The tail depends on s0 (struct pointer, = entry a0), on the byte 0x2E as v1, and on a0 = byte 0x2D
   while no call has happened yet. All of it is inlined here. */
uint32_t man_800A1CFC(const GuestMem *m, uint32_t a0) {
    uint32_t s0 = a0;
    uint32_t a0b;
    uint32_t p;
    uint32_t r;
    int32_t v1;

    /* 800A1D0C..800A1D14: lb v0,0x2d(s0); beq v0,zero -> 800A1F0C (return 0, no store) */
    if (RDS8(s0 + 0x2Du) == 0) {
        return 0u;
    }
    /* 800A1D1C..800A1D24: v1 = lb 0x2e(s0); bne v1,zero -> 800A1D88 */
    v1 = RDS8(s0 + 0x2Eu);

    if (v1 == 0) {
        /* 800A1D2C..800A1D38: halfword 0x80066758 is read before any call, then branches */
        uint32_t hw = RD16(0x80066758u);
        m->call4(m->ctx, 0x8001DFDCu, 0x17u, 0u, 0u, 0u);
        m->call4(m->ctx, 0x8001C9FCu, 0u, 0u, 0u, 0u);
        if (hw == 0u) {
            /* 800A1D60..800A1D80: same call, then the 800A1D68 tail: d194 before 800A1F00 */
            m->call4(m->ctx, 0x8001E458u, 0u, 0u, 0u, 0u);
            m->call4(m->ctx, 0x8001D194u, 0u, 0u, 0u, 0u);
        } else {
            m->call4(m->ctx, 0x8001E458u, 0u, 0u, 0u, 0u);
        }
        /* 800A1F00: jal 8001DF98, then 800A1F08: sb zero,0x2d(s0); v0 = df98 result */
        r = m->call4(m->ctx, 0x8001DF98u, 0u, 0u, 0u, 0u);
        WR8(s0 + 0x2Du, 0u);
        return r;
    }

    /* From here 800A1D88: a0 = lbu 0x2d(s0), still live (no call yet) */
    a0b = RD8(s0 + 0x2Du);

    if (v1 == 2) {
        /* 800A1D90..800A1DA4 */
        if ((a0b & 1u) == 0u) {
            WR8(s0 + 0x2Du, 0u);
            return 0u;
        }
        r = m->call4(m->ctx, 0x8001E458u, a0b, 0u, 0u, 0u);
        WR8(s0 + 0x2Du, 0u);
        return r;
    }

    if (v1 == 3) {
        /* 800A1DB4..800A1DF0 */
        m->call4(m->ctx, 0x8001DDB8u, a0b, 0u, 0u, 0u);
        if ((RD8(s0 + 0x2Du) & 1u) != 0u) {
            m->call4(m->ctx, 0x8001E458u, 0u, 0u, 0u, 0u);
        }
        p = RD32(0x800A34E8u);
        m->call4(m->ctx, 0x8001AF08u, RD8(p + 4u), 0u, 0u, 0u);
        r = m->call4(m->ctx, 0x8001DF98u, 0u, 0u, 0u, 0u);
        WR8(s0 + 0x2Du, 0u);
        return r;
    }

    if (v1 == 4) {
        /* 800A1DFC..800A1E10: e3fb4, e3e08, then 800A1F0C with sb in the delay slot */
        m->call4(m->ctx, 0x800E3FB4u, a0b, 0u, 0u, 0u);
        r = m->call4(m->ctx, 0x800E3E08u, 0u, 0u, 0u, 0u);
        WR8(s0 + 0x2Du, 0u);
        return r;
    }

    if (v1 == 5) {
        /* 800A1E20..800A1E74 */
        p = RD32(0x800A34E8u);
        if (RDS8(p + 0u) == 0) {
            uint32_t a0x = (RDS8(p + 1u) == 16) ? 6u : RD8(p + 1u);
            m->call4(m->ctx, 0x8001DFDCu, a0x, 0u, 0u, 0u);
        } else {
            /* a1 still holds the pointer p (lw a1 at 800A1E24) on this path */
            m->call4(m->ctx, 0x8001DDB8u, a0b, p, 0u, 0u);
        }
        m->call4(m->ctx, 0x8001E3B4u, 0u, 0u, 0u, 0u);
        r = m->call4(m->ctx, 0x8001DF98u, 0u, 0u, 0u, 0u);
        WR8(s0 + 0x2Du, 0u);
        return r;
    }

    if (v1 == 6) {
        /* 800A1E7C..800A1EA8 */
        m->call4(m->ctx, 0x8001DDB8u, a0b, 0u, 0u, 0u);
        m->call4(m->ctx, 0x8001C9FCu, 0u, 0u, 0u, 0u);
        m->call4(m->ctx, 0x8001E480u, 0u, 0u, 0u, 0u);
        m->call4(m->ctx, 0x8002C7ACu, 6u, 0u, 0u, 0u);
        m->call4(m->ctx, 0x8001BA94u, 0u, 0u, 0u, 0u);
        r = m->call4(m->ctx, 0x8001DF98u, 0u, 0u, 0u, 0u);
        WR8(s0 + 0x2Du, 0u);
        return r;
    }

    if (v1 == 7) {
        /* 800A1EB4..800A1EC8 */
        m->call4(m->ctx, 0x800E5660u, a0b, 0u, 0u, 0u);
        r = m->call4(m->ctx, 0x800E548Cu, 0u, 0u, 0u, 0u);
        WR8(s0 + 0x2Du, 0u);
        return r;
    }

    /* 800A1ED0..800A1EE0: v1 == 8 returns 9, v1 == 9 returns 10 (delay slots set v0), others return 10 */
    if (v1 == 8) {
        WR8(s0 + 0x2Du, 0u);
        return 9u;
    }
    if (v1 == 10) {
        /* 800A1EE8..800A1F04 */
        m->call4(m->ctx, 0x8001DDB8u, a0b, 0u, 0u, 0u);
        m->call4(m->ctx, 0x8001E6DCu, 0u, 0u, 0u, 0u);
        m->call4(m->ctx, 0x800F4B28u, 0u, 0u, 0u, 0u);
        r = m->call4(m->ctx, 0x8001DF98u, 0u, 0u, 0u, 0u);
        WR8(s0 + 0x2Du, 0u);
        return r;
    }
    WR8(s0 + 0x2Du, 0u);
    return 10u;
}
