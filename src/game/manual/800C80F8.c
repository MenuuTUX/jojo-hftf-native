#include "manual.h"
#include "hex2c_rt.h"

/* 800C80F8: frame of 0x40 bytes. The fifth argument (byte a5) is the caller's stack byte at
   sp+0x10, i.e. 0x50(sp) of this frame. The low byte of a1 is stored at the local sp+0x10 and
   read back after the calls. The frame is reproduced in guest memory for that local.
   Returns $v0 as the original leaves it: 0 on the a1 & 0x7F == 0x7F loop path and on the
   early-exit paths, otherwise 407, 392 or 393 (from the low 7 bits of the stored byte). */
uint32_t man_800C80F8(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx) - 0x40u;
    uint32_t s0;
    uint32_t s1 = a0;
    uint32_t s2;
    uint32_t s3 = a2;
    uint32_t s4 = a1;
    uint32_t s6 = a3;
    uint32_t s7;
    uint32_t t0;
    uint32_t cnt;
    uint32_t v;

    m->sp_set(m->ctx, sp);
    /* 800C8138: lbu s7, 0x50(sp) (the caller's fifth argument) */
    s7 = RD8(sp + 0x50u);
    /* 800C814C: sb s4, 0x10(sp) (delay slot of the bne, runs on both paths) */
    WR8(sp + 0x10u, s4);
    s2 = s4 & 0x7Fu;

    if (s2 == 0x7Fu) {
        /* 800C8150..800C81E8: loop over slots 1 and 2 */
        s4 &= 0x80u;
        s2 = 1u;
        do {
            s0 = m->call4(m->ctx, 0x80025548u, 6u, 0u, s2 & 0xFFu, a3);
            if (s0 != 0u) {
                WR8(s0 + 0x32u, s7);
                WR16(s0 + 0x1Eu, 393u);
                WR8(s0 + 0x13u, RD8(s1 + 0x13u));
                if (s4 != 0u) {
                    WR16(s0 + 0x6Au, s3);
                    WR16(s0 + 0x6Eu, s6);
                } else {
                    if (RD8(s1 + 0x0Bu) != 0u) {
                        WR16(s0 + 0x6Au, RD16(s1 + 0x6Au) + s3);
                    } else {
                        WR16(s0 + 0x6Au, RD16(s1 + 0x6Au) - s3);
                    }
                    WR16(s0 + 0x6Eu, RD16(s1 + 0x6Eu) + s6);
                }
            }
            s2 = (s2 + 1u) & 0xFFFFu;
        } while ((s2 & 0xFFFFu) < 3u);
        /* the loop exits with $v0 = 0 (sltiu) */
        m->sp_set(m->ctx, sp + 0x40u);
        return 0u;
    }

    /* 800C81EC..800C82C8: single object path */
    s0 = m->call4(m->ctx, 0x80025548u, 6u, 0u, 0u, a3);
    if (s0 == 0u) {
        m->sp_set(m->ctx, sp + 0x40u);
        return 0u;
    }
    /* 800C820C: sb s2, 0x88(s0) runs in the delay slot of the first jal */
    WR8(s0 + 0x88u, s2);
    cnt = m->call4(m->ctx, 0x80025BF8u, 0u, 0u, 0u, 0u);
    /* 800C8214: sh v0, 0xC2(s0) with v0 = the first result, in the delay slot of the second jal */
    WR16(s0 + 0xC2u, cnt);
    cnt = m->call4(m->ctx, 0x80025BF8u, 0u, 0u, 0u, 0u);
    /* 800C8220: sb (result & 1), 0x8A(s0) in the delay slot of the third jal */
    WR8(s0 + 0x8Au, cnt & 1u);
    cnt = m->call4(m->ctx, 0x80025BF8u, 0u, 0u, 0u, 0u);
    /* 800C822C: sb ((result & 3) - 1), 0x8B(s0) */
    WR8(s0 + 0x8Bu, (cnt & 3u) - 1u);

    /* 800C8230..800C8238 */
    WR8(s0 + 0x0Bu, RD8(s1 + 0x0Bu));

    /* 800C823C..800C828C: position fields, from a3/a2 directly or from struct A */
    if ((s4 & 0x80u) != 0u) {
        WR16(s0 + 0x6Au, s3);
        WR16(s0 + 0x6Eu, s6);
    } else {
        if (RD8(s1 + 0x0Bu) != 0u) {
            WR16(s0 + 0x6Au, RD16(s1 + 0x6Au) + s3);
        } else {
            WR16(s0 + 0x6Au, RD16(s1 + 0x6Au) - s3);
        }
        WR16(s0 + 0x6Eu, RD16(s1 + 0x6Eu) + s6);
    }

    /* 800C8290..800C8298: lbu 0x13(s1) is read before sb s7 and sb 0x13 */
    cnt = RD8(s1 + 0x13u);
    WR8(s0 + 0x32u, s7);
    WR8(s0 + 0x13u, cnt);

    /* 800C829C..800C82C4: result type from the stored byte at sp+0x10 */
    t0 = RD8(sp + 0x10u);
    if ((t0 & 0x7Fu) == 3u) {
        v = 407u;
    } else if ((t0 & 0x7Fu) == 4u) {
        v = 393u;
    } else {
        v = 392u;
    }
    WR16(s0 + 0x1Eu, v);

    m->sp_set(m->ctx, sp + 0x40u);
    return v;
}
