#include "manual.h"
#include "hex2c_rt.h"

/* 80018FEC (TEXT). Frame 0x30. Local RECT at sp+0x10 (x, y, w, h as halfwords), passed to 80036348 (LoadImage) and read
 * back after the call. $a2 and $a3 are never written, so they reach the callee unchanged: nargs is 4. $v0 is written before
 * any read. Table base $s0 = 0x80079208, entries of 56 bytes. The return value is the $v0 left at jr ra on the path taken. */
uint32_t man_80018FEC(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const uint32_t S = 0x80079208u;   /* $s0 */
    uint32_t sp = m->sp_get(m->ctx) - 0x30u;
    uint32_t v0 = 0u, s1 = 0u, s2 = 0u, s3 = 0u, s4 = 0u;
    uint32_t idx, e, cnt, a1v, bx, b59, b80;
    int32_t ws, hs;

    m->sp_set(m->ctx, sp);

    do {
        /* 80019010 */
        s3 = (RD8(S + 0x82u) - RD8(S + 0x83u)) & 7u;
        v0 = RD8(S + 0x33u);
        if (v0 != 0u && s3 == 0u)
            s3 = 8u;
        if ((int32_t)s3 <= 0) {       /* blez at 8001903c; its delay slot sets $s4 = 0 */
            s4 = 0u;
            continue;
        }
        s4 = 0u;

        for (;;) {
            /* 80019044 */
            s2 = 0x80111800u;
            bx = RD8(S + 0x83u);
            idx = RD8(S + 0x81u);
            v0 = 1u;
            WR8(S + 0x34u, v0);
            e = S + 56u * idx;
            s1 = RD32(e + 0x88u);
            if (!((int32_t)s1 < 2048))
                s1 = 2048u;
            s2 += bx << 11;

            while ((int32_t)s1 > 0) {
                /* 80019094 */
                e = S + 56u * RD8(S + 0x81u);
                WR16(sp + 0x10u, RD16(e + 0x8Cu));
                WR16(sp + 0x12u, RD16(e + 0x8Eu));
                WR16(sp + 0x14u, RD16(e + 0x90u));
                WR16(sp + 0x16u, RD16(e + 0x92u));
                m->call4(m->ctx, 0x80036348u, sp + 0x10u, s2, a2, a3);

                ws = RDS16(sp + 0x14u);
                hs = RDS16(sp + 0x16u);
                a1v = (uint32_t)((int64_t)ws * (int64_t)(hs * 2));   /* mult a1, h<<1; mflo */

                e = S + 56u * RD8(S + 0x81u);
                cnt = RD32(e + 0x88u) - a1v;
                WR32(e + 0x88u, cnt);

                e = S + 56u * RD8(S + 0x81u);
                WR32(e + 0x98u, RD32(e + 0x98u) + a1v);

                e = S + 56u * RD8(S + 0x81u);
                WR16(e + 0x8Cu, RD16(e + 0x8Cu) + RD16(e + 0x94u));
                s1 -= a1v;

                e = S + 56u * RD8(S + 0x81u);
                WR16(e + 0x8Eu, RD16(e + 0x8Eu) + RD16(e + 0x96u));
                s2 += a1v;
            }

            /* 800191d0 */
            e = S + 56u * RD8(S + 0x81u);
            if (RD32(e + 0x9Cu) == RD32(e + 0x98u)) {
                WR32(e + 0x98u, 0u);
                WR16(e + 0x8Cu, RD16(e + 0x8Cu) + RD16(e + 0xA0u));
                WR16(e + 0x8Eu, RD16(e + 0x8Eu) + RD16(e + 0xA2u));
                WR16(e + 0x90u, RD16(e + 0x90u) + RD16(e + 0xA4u));
                WR16(e + 0x92u, RD16(e + 0x92u) + RD16(e + 0xA6u));
                WR16(e + 0x94u, RD16(e + 0x94u) + RD16(e + 0xA8u));
                WR16(e + 0x96u, RD16(e + 0x96u) + RD16(e + 0xAAu));
            }

            /* 80019308 */
            WR8(S + 0x83u, (RD8(S + 0x83u) + 1u) & 7u);
            e = S + 56u * RD8(S + 0x81u);
            v0 = RD32(e + 0x88u);
            if ((int32_t)v0 > 0) {
                /* 8001936c */
                s4 += 1u;
                v0 = ((int32_t)s4 < (int32_t)s3) ? 1u : 0u;
                WR16(S + 0x42u, 0u);          /* sh zero in the bne delay slot: runs on both outcomes */
                if (v0 != 0u)
                    continue;                 /* bne to 80019044 */
                break;                        /* fall through to 8001937c */
            }

            /* 8001933c: the counter is exhausted, leave the inner loop through 8001937c */
            WR16(e + 0x84u, 0u);
            v0 = RD8(S + 0x81u) + 1u;
            b59 = RD8(S + 0x59u);
            WR8(S + 0x81u, v0);               /* sb in the delay slot of bne at 8001934c */
            if (b59 != 0u)
                goto outer_test;
            b80 = RD8(S + 0x80u);
            v0 &= 0xFFu;
            if (v0 != b80) {
                v0 = 1u;                  /* addiu in the delay slot of the bne at 8001935c: runs on both outcomes */
                goto outer_test;
            }
            v0 = 1u;                      /* 80019360, then j 8001937c with sb in its delay slot */
            WR8(S + 0x3Fu, v0);
            goto outer_test;
        }

    outer_test: ;
    } while (s3 != s4);

    WR8(S + 0x34u, 0u);
    m->sp_set(m->ctx, sp + 0x30u);
    return v0;
}
