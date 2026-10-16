#include "manual.h"
#include "hex2c_rt.h"

/* 800BE278: state machine on the byte at +8 (jump table at 0x800A6740, states 0..7; 8 and above return 0x800A0000,
 * the lui in the beq delay slot). State 0 runs its setup and falls into state 1. $a1 is passed on unchanged to the
 * first callee of states 6 and 7; $a2 and $a3 are passed on until the first call of each path (then they are zero,
 * as the callee clobbers them). The state-2/3/4/5 flag is byte 0x80066C61 (a0 = 0x80066C48 set at dispatch). */
uint32_t man_800BE278(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;
    uint32_t st, s1, r, n, t, t1, t2, nv, addr, tbl, sum, v, b, ee, f0, a2v, a1x;

    WR8(p + 0x107, 3);
    s1 = RD32(p + 0x108);
    st = RD8(p + 8);
    if (st >= 8) {
        return 0x800A0000u;
    }

    if (st == 0) {
        m->call4(m->ctx, 0x800BC654u, p, 24, a2, a3);
        WR8(p + 8, RD8(p + 8) + 1u);
        m->call4(m->ctx, 0x800BCC04u, p, 85, 0, 0);
        WR8(p + 0x201, 1);
        m->call4(m->ctx, 0x800B98C4u, p, 0, 0, 0);
        WR8(p + 0xB0, 0x83);
        WR8(p + 0xE8, 0x80);
        WR8(p + 0xE4, 0x80);
        WR8(p + 0xE0, 0x80);
        WR8(p + 0xF8, 0);
        WR8(p + 0xF7, 0);
        WR8(p + 0xF6, 0);
        WR16(p + 0xF0, 63);
        WR16(p + 0xEE, 63);
        WR8(p + 0xB8, 0);
        a2 = 0;
        a3 = 0;
    }

    /* state 1 (also the fall-through of state 0) */
    if (st <= 1) {
        if (RDS8(p + 0x145) <= 0) {
            m->call4(m->ctx, 0x80024104u, s1, 294, a2, a3);
            n = RD8(p + 8) + 1u;
            WR16(p + 0xB4, 0);
            WR16(p + 0xC, 3);
            WR16(p + 0xE, 0);
            WR8(p + 8, n);
            return 3;
        }
        tbl = 0x800CBA3Cu + (uint32_t)(int32_t)RDS16(p + 0xC);
        n = (RD16(p + 0xC) + 1u) & 0xFu;
        b = RD8(tbl);
        WR16(p + 0xC, n);
        WR8(p + 0x145, RD8(p + 0x145) - 1u);
        sum = (uint32_t)RD16(p + 0xB4) + (uint32_t)(int32_t)(int8_t)b;
        WR16(p + 0xB4, sum);
        return sum;
    }

    if (st == 2) {
        t = RD16(p + 0x84);
        if (RD8(0x80066C61u) & 1u) {
            t += 4u;
        }
        WR16(p + 0x1E, t);
        addr = 0x800CBC24u + (uint32_t)((int32_t)RDS16(p + 0xE) * 2);
        if ((int32_t)RDS8(addr) == -1) {
            n = RD8(p + 8) + 1u;
            WR8(p + 8, n);
            return n;
        }
        v = RD16(p + 0xC) - 1u;
        WR16(p + 0xC, v);
        if ((v << 16) != 0u) {
            return v << 16;
        }
        WR16(p + 0xC, 2);
        t1 = RD16(addr);
        WR16(p + 0xEE, t1);
        nv = RD16(p + 0xE) + 1u;
        WR16(p + 0xE, nv);
        t2 = RD16(addr);
        WR16(p + 0xF0, t2);
        return nv;
    }

    if (st == 3) {
        t = RD16(p + 0x84);
        if (RD8(0x80066C61u) & 1u) {
            t += 4u;
        }
        WR16(p + 0x1E, t);
        a2v = 8;
        if (RDS8(p + 0x136) != 0) {
            a2v = 7;
        }
        WR8(p + 8, RD8(p + 8) + 1u);
        WR8(p + 0xB1, a2v);
        WR16(p + 0x1E, RD16(p + 0x84) + 5u);
        return m->call4(m->ctx, 0x800B4AD0u, p, 8, a2v, a3);
    }

    if (st == 4) {
        t = RD16(p + 0x84);
        t += (RD8(0x80066C61u) & 1u) ? 4u : 5u;
        WR16(p + 0x1E, t);
        ee = RD16(p + 0xEE) + 8u;
        WR16(p + 0xEE, ee);
        if ((ee & 0xFFFFu) >= 97u) {
            WR16(p + 0xEE, 96);
        }
        f0 = RD16(p + 0xF0) + 8u;
        WR16(p + 0xF0, f0);
        if ((f0 & 0xFFFFu) < 96u) {
            return 1;
        }
        WR16(p + 0xF0, 96);
        n = RD8(p + 8) + 1u;
        WR8(p + 8, n);
        return n;
    }

    if (st == 5) {
        t = RD16(p + 0x84);
        t += (RD8(0x80066C61u) & 1u) ? 4u : 5u;
        WR16(p + 0x1E, t);
        ee = RD16(p + 0xEE) - 8u;
        WR16(p + 0xEE, ee);
        if ((ee & 0xFFFFu) < 63u) {
            WR16(p + 0xEE, 63);
        }
        f0 = RD16(p + 0xF0) - 8u;
        WR16(p + 0xF0, f0);
        if ((f0 & 0xFFFFu) >= 64u) {
            return 0;
        }
        WR16(p + 0xF0, 63);
        WR32(p + 0x70, 0x20000u);
        n = RD8(p + 8) + 1u;
        WR32(p + 0x74, 0x88000u);
        WR32(p + 0x7C, 0xFFFF8800u);
        b = RD8(p + 0xB);
        WR8(p + 8, n);
        WR16(p + 0x1E, RD16(p + 0x84) + 5u);
        WR32(p + 0x78, 0);
        if (b != 0) {
            WR32(p + 0x70, 0u - RD32(p + 0x70));
            WR32(p + 0x78, 0u - RD32(p + 0x78));
        }
        return m->call4(m->ctx, 0x800B4AD0u, p, 8, a2, a3);
    }

    /* states 6 and 7 */
    if (st == 6 || st == 7) {
        m->call4(m->ctx, 0x800B428Cu, p, a1, a2, a3);
        m->call4(m->ctx, 0x80026A04u, p, 0, 0, 0);
        r = m->call4(m->ctx, 0x800B2738u, p, 0, 0, 0);
        if (r == 0) {
            return r;
        }
        WR16(p + 0x6E, 35);
        WR8(p + 8, RD8(p + 8) + 1u);
        m->call4(m->ctx, 0x80024564u, p, 0, 0, 0);
        if (st == 6) {
            WR32(p + 0x70, 0x20000u);
            WR32(p + 0x74, 0x40000u);
            WR32(p + 0x78, 0);
            WR32(p + 0x7C, 0xFFFF8800u);
            if (RD8(p + 0xB) == 0) {
                return 0xFFFF8800u;
            }
            v = 0u - RD32(p + 0x70);
            WR32(p + 0x70, v);
            WR32(p + 0x78, 0u - RD32(p + 0x78));
            return v;
        }
        a1x = RD8(p + 0xBA);
        WR16(p + 0x258, 300);
        WR8(p + 0xB0, a1x);
        WR16(p + 0x1E, RD16(p + 0x84) + 5u);
        return m->call4(m->ctx, 0x800AF7ECu, p, a1x, 0, 0);
    }

    /* not reached: st is below 8 and every state above returned */
    return 0x800A0000u;
}
