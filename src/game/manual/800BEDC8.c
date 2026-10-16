#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800BEDC8(const GuestMem *m, uint32_t a0) {
    uint32_t p = a0;
    uint32_t state = RD8(p + 8);            /* lbu a1,0x8(s0) at 800bedf0 */
    uint32_t w = RD32(p + 0x170);           /* s1 */
    uint32_t x = RD32(p + 0x17C);           /* s2 */
    uint32_t v, v0, t, t136, tbl, target, r;
    int32_t sv;

    WR8(p + 1, 1);                          /* sb s4,0x1(s0): delay slot of bne at 800bee04 */

    if (state == 0) {
        WR8(p + 8, state + 1);              /* sb v0,0x8(s0) at 800bee14 */
        WR16(p + 0x16A, 0xFFFF);
        if ((int16_t)RD16(w + 0x16A) < 0) {
            WR16(w + 0x16A, 0xFFFF);
        }
        m->call4(m->ctx, 0x800B98C4u, p, state, 0, 0);
        m->call4(m->ctx, 0x800BB27Cu, p, state, 0, 0);
        m->call4(m->ctx, 0x800BB27Cu, w, state, 0, 0);
        m->call4(m->ctx, 0x800AE1B8u, w, state, 0, 0);
        v = RD8(p + 0xBA);
        t = RD16(p + 0x84);
        t136 = (uint32_t)(int32_t)RDS8(p + 0x136);
        WR8(p + 0x201, 0);
        WR8(p + 0xB0, v);
        WR16(p + 0x1E, t);
        WR8(p + 0xB1, t136 != 0);           /* sltu v0,zero,v0 */
        WR8(w + 0x203, 0);
        WR8(p + 0x203, 0);
        WR8(w + 0x202, 0);
        v = RD32(p + 0x130);                /* lw v0,0x130(s0) at 800bee7c */
        WR8(p + 0x202, 0);
        WR8(p + 0xB9, 1);
        t = RD8(v + 0x33);
        if (t != 2 && t != 4) {
            WR8(w + 0xB9, 0);
        }
        WR8(p + 0x25C, 0);
        WR8(w + 0x25C, 0);
        WR16(p + 0xB6, 0);
        WR16(p + 0xB4, 0);
        WR16(w + 0xB6, 0);
        WR16(w + 0xB4, 0);
        WR16(p + 0xC, 0);
        WR8(p + 0x1FC, 0);
        WR16(p + 0x1E, RD16(p + 0x84));
        WR16(w + 0x1E, RD16(w + 0x84));
        sv = RDS8(p + 0x137);
        WR8(p + 0x163, 0);
        WR8(p + 0x162, 0);
        WR8(p + 0x231, 0);
        WR8(p + 0x221, 0);
        WR8(p + 0x255, 0);
        WR8(p + 0x204, 0);
        if (sv != 8) {
            WR8(w + 1, 0);
            WR8(w + 0x204, 0);
        }
        return m->call4(m->ctx, 0x800BCC04u, p, 1, 0, 0);
    }

    if (state == 1) {
        WR8(p + 8, state + 1);              /* sb v0,0x8(s0) at 800bef30, $v0 = state + 1 */
        if (RD8(p + 0x158) == 0) {
            m->call4(m->ctx, 0x800C7594u, p, state, 0, 0);
        }
        v = RD32(x + 0x130);
        tbl = RD8(0x800CBD58u + RD8(0x80066CA6u));
        if (RD8(v + 0x23) < tbl && RD8(0x8006D2D1u) != 9) {
            WR8(0x80066CC6u, 60);
            return m->call4(m->ctx, 0x80024104u, p, 265, 0, 0);
        }
        WR8(0x80066CC6u, 120);
        return m->call4(m->ctx, 0x80024104u, p, 256, 0, 0);
    }

    if (state == 2) {
        sv = RDS8(p + 0x145);
        if (sv > 0) {
            tbl = RD8(0x800CBA3Cu + (uint32_t)(int32_t)RDS16(p + 0xC));
            WR16(p + 0xC, (RD16(p + 0xC) + 1u) & 0xFu);
            WR8(p + 0x145, RD8(p + 0x145) - 1u);
            v0 = RD16(p + 0x6A) + (uint32_t)(int32_t)(int8_t)(uint8_t)tbl;
            WR16(p + 0x6A, v0);
            return v0;
        }
        v = RD8(p + 0x106);
        WR8(p + 8, state + 1);              /* sb v0,0x8(s0) at 800bf01c */
        WR32(p + 0x74, 0x50000u);
        WR32(p + 0x7C, 0xFFFFC000u);
        WR32(p + 0x70, 0x38000u);
        WR32(p + 0x78, 0);
        if (v == 0) {
            WR32(p + 0x70, 0u - 0x38000u);
            WR32(p + 0x78, 0);
        }
        sv = RDS8(p + 0x137);
        if (sv == 21) {
            WR8(p + 8, 5);                  /* sb v1,0x8(s0): delay slot of j 800bf158 */
            target = RD32(RD32(p + 0x12C) + 0x58u);
            return m->calli(m->ctx, target, p, state, 0, 0);   /* a1 still 2 */
        }
        return m->call4(m->ctx, 0x800BCC04u, p, 63, 0, 0);
    }

    if (state == 3) {
        m->call4(m->ctx, 0x800B428Cu, p, state, 0, 0);
        m->call4(m->ctx, 0x80026A04u, p, state, 0, 0);
        sv = RDS16(p + 0x76);
        if (sv >= 0) {
            return (uint32_t)(int32_t)sv;   /* bgez at 800bf0a4 */
        }
        WR8(p + 8, RD8(p + 8) + 1u);        /* sb v0,0x8(a0): delay slot of jal 800bcc04 */
        return m->call4(m->ctx, 0x800BCC04u, p, 64, 0, 0);
    }

    if (state == 4) {
        m->call4(m->ctx, 0x800B428Cu, p, state, 0, 0);
        m->call4(m->ctx, 0x80026A04u, p, state, 0, 0);
        r = m->call4(m->ctx, 0x800B2738u, p, state, 0, 0);
        if (r == 0) {
            return 0;
        }
        WR8(p + 8, RD8(p + 8) + 1u);
        sv = RDS8(p + 0x137);
        if (sv != 13 && sv != 21) {
            m->call4(m->ctx, 0x80024564u, p, state, 0, 0);
            m->call4(m->ctx, 0x8002B450u, 4, 2, 0, 0);
            m->call4(m->ctx, 0x800C7374u, p, 0, 0, 0);
        }
        WR16(p + 0x6E, 35);                 /* sh v0,0x6e(s0): delay slot of beq at 800bf140 */
        sv = RDS8(p + 0x137);
        if (sv == 0 || sv == 24) {
            /* a0 = s0 from the delay slot of jalr (800bf168); a1 = 0 left by the call at 800bf130 (GUESS: callee preserves it) */
            target = RD32(RD32(p + 0x12C) + 0x58u);
            return m->calli(m->ctx, target, p, 0, 0, 0);
        }
        return m->call4(m->ctx, 0x800BCC04u, p, 65, 0, 0);
    }

    if (state == 5) {
        sv = RDS8(p + 0x4D);
        if (sv == -1) {
            WR8(p + 8, 6);                  /* sb v0,0x8(s0) at 800bf19c, $v0 = state + 1 */
            WR8(0x80066CC6u, 0);
            WR8(0x80066CA7u, RD8(0x80066CA7u) | (RD8(p + 2) + 1u));
        }
        return m->call4(m->ctx, 0x80026A04u, p, 5, 0, 0);   /* $v0 entry = 6 (not passed) */
    }

    return 0xFFFFFFFFu;                     /* addiu v0,zero,-1 in the delay slot of bne at 800bf184 */
}
