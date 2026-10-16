#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800121F0(const GuestMem *m, uint32_t a0, uint32_t a1);

/* 800C6180 (AC_MAIN). Frame: addiu sp,sp,-64. The record at sp+0x10 (halfword x, halfword 24, halfword y, two zero
 * bytes) is the buffer passed to 800121F0 with the byte table at 0x800DAE68. The three loops are the tens, fives and
 * ones of s2. Returns the $v0 the original leaves at jr ra on each exit. */
uint32_t man_800C6180(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx) - 0x40u;
    uint32_t s4 = a0;
    uint32_t s2 = 0u;
    uint32_t s3 = 0u;
    uint32_t fp;
    uint32_t s1;
    uint32_t v0 = 0u;
    uint32_t target;
    int32_t k;
    int32_t n;
    int32_t q10;
    int32_t r;
    int32_t q5;
    int32_t r2;
    int8_t v3;
    uint8_t b5;

    m->sp_set(m->ctx, sp);

    /* 800C6184: lbu v0 = byte_80066C6F; zero -> return 0 */
    v0 = RD8(0x80066C6Fu);
    if (v0 == 0u) {
        m->sp_set(m->ctx, sp + 0x40u);
        return 0u;
    }

    v3 = RDS8(s4 + 0x137u);
    b5 = RD8(s4 + 5u);
    if (v3 == 7 && b5 != 0u) {
        /* 800C61DC..800C61F0: target = *(*(*(a1+368)+300)+100), a0..a3 unchanged */
        target = RD32(RD32(RD32(s4 + 0x170u) + 0x12Cu) + 0x64u);
        s2 = m->calli(m->ctx, target, a0, a1, a2, a3);
        v0 = s2;
    } else if ((v3 == 19 || v3 == 25) && b5 != 0u) {
        /* 800C6234..800C6244: target = *(*(a1+300)+100), a0 = a1 */
        target = RD32(RD32(s4 + 0x12Cu) + 0x64u);
        s2 = m->calli(m->ctx, target, a0, a1, a2, a3);
        v0 = s2;
    } else {
        /* 800C6214..800C6250: v0 = 25, or byte_5 (zero here) when the state is 25 */
        v0 = (v3 == 25) ? (uint32_t)b5 : 25u;
    }

    if (s2 == 0u) {
        m->sp_set(m->ctx, sp + 0x40u);
        return v0;
    }

    /* 800C6258..800C6268: fp = byte_136 != 0 ? 280 : 56 */
    fp = (RD8(s4 + 0x136u) != 0u) ? 280u : 56u;

    /* 800C6274..800C6284: n / 10 by the magic multiply (signed) */
    n = (int32_t)s2;
    q10 = n / 10;
    if (q10 > 0) {
        /* loop 1: 800C62A0..800C62F0 */
        s1 = fp;
        for (k = 0; k < q10; k++) {
            WR16(sp + 0x10u, s1);
            s1 += 8u;
            s3 += 1u;
            WR8(0x800DAE68u, 0xF6u);
            WR8(0x800DAE69u, 0u);
            WR16(sp + 0x12u, 24u);
            WR8(sp + 0x16u, 0u);
            WR8(sp + 0x17u, 0u);
            WR16(sp + 0x14u, (uint32_t)(RDS8(s4 + 0x136u) + 20));
            man_800121F0(m, sp + 0x10u, 0x800DAE68u);
        }
    }

    /* 800C62F8..800C6320: r = n - 10 * (n / 10) */
    r = n - 10 * q10;
    /* 800C6324..800C6330: q5 = r / 5 */
    q5 = r / 5;
    if (q5 > 0) {
        /* loop 2: 800C6344..800C63A4 */
        s1 = (s3 << 3) + fp;
        for (k = 0; k < q5; k++) {
            WR16(sp + 0x10u, s1);
            s1 += 8u;
            s3 += 1u;
            WR8(0x800DAE68u, 0xF7u);
            WR8(0x800DAE69u, 0u);
            WR16(sp + 0x12u, 24u);
            WR8(sp + 0x16u, 0u);
            WR8(sp + 0x17u, 0u);
            WR16(sp + 0x14u, (uint32_t)(RDS8(s4 + 0x136u) + 20));
            man_800121F0(m, sp + 0x10u, 0x800DAE68u);
        }
    }

    /* 800C63AC..800C63D4: r2 = r - 5 * q5; v0 = 5 * q5 when r2 <= 0 */
    r2 = r - 5 * q5;
    if (r2 <= 0) {
        v0 = (uint32_t)(5 * q5);
        m->sp_set(m->ctx, sp + 0x40u);
        return v0;
    }

    /* loop 3: 800C63E4..800C6434, do-while over r2; the last slt leaves $v0 = 0 */
    s1 = (s3 << 3) + fp;
    k = 0;
    do {
        WR16(sp + 0x10u, s1);
        s1 += 8u;
        WR8(0x800DAE68u, 0xF8u);
        WR8(0x800DAE69u, 0u);
        WR16(sp + 0x12u, 24u);
        WR8(sp + 0x16u, 0u);
        WR8(sp + 0x17u, 0u);
        WR16(sp + 0x14u, (uint32_t)(RDS8(s4 + 0x136u) + 20));
        man_800121F0(m, sp + 0x10u, 0x800DAE68u);
        k++;
    } while (k < r2);

    v0 = 0u;
    m->sp_set(m->ctx, sp + 0x40u);
    return v0;
}
