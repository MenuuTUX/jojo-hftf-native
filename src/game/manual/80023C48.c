#include "manual.h"
#include "hex2c_rt.h"

#define SNDB 0x8008A000u   /* $s4 in the original: byte_8008A000 */

uint32_t man_80023C48(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);

/* Voice command dispatcher (segment TEXT). Top nibble of the word at a0 selects the case: 0 key on, 8 loop over a list,
 * 9 volume, 11 key off; the others return 0 (12..15) or the jump-table target 0x8002409C (1..7, 10). The original frame is
 * 0x50 bytes: the 5th and 6th arguments are at sp+0x60 and sp+0x64, and the outgoing stack words at sp+0x10..0x1C are for
 * the 8-argument call to 8003FA90 and for the recursive call. */
uint32_t man_80023C48(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx) - 0x50u;   /* 0x50 = the original "addiu sp,sp,-80" */
    uint32_t p = a0;                           /* $s1 */
    uint32_t idx;
    uint32_t v0 = 0u;
    uint32_t fp, s6;                           /* 5th argument (halfword), 6th argument (byte) */
    uint32_t v11, v12, s2, vb, hw, r;
    uint32_t a1x, a2b, a3b, t0, t1, t2;
    uint32_t v21, b2, flag, ix, addr, s0, v25, fl, bit, vp, vq, x;
    int32_t a6s, sum, pa, pb, pr, xi, ax, vol;

    m->sp_set(m->ctx, sp);
    fp = RD16(sp + 0x60u);
    s6 = RD8(sp + 0x64u);
    idx = (RD32(p) >> 12) & 0xFu;

    switch (idx) {
    case 0:
        v11 = RD32(p + 4u);
        v12 = (v11 >> 7) & 0x1Fu;
        if (a2 != 0u) v12 += 10u;
        s2 = SNDB + v12;
        vb = RD8(s2 + 0x4B5Cu);
        hw = (v11 >> 16) & 0x7Fu;
        if ((int32_t)hw < (int32_t)vb) {
            r = m->call4(m->ctx, 0x800440B0u, 1u << (v12 & 31u), a1, a2, a3);
            if (r == 1u) {
                v0 = r;
                goto done;
            }
        }
        WR8(s2 + 0x4B5Cu, RD16(p + 6u) & 0x7Fu);
        a1x = (uint32_t)(int32_t)RDS8(SNDB + a1);
        v11 = RD32(p + 4u);
        a2b = RD8(p + 3u);
        a3b = (v11 >> 12) & 0xFu;
        t1 = v11 & 0x7Fu;
        t2 = (v11 >> 24) & 0x7Fu;
        a6s = (int32_t)(int8_t)s6;
        t0 = t1;
        if (a6s != -1) {
            if (a6s < 2) {
                v21 = RD8(0x8005A7DCu + RD8(SNDB + (uint32_t)a6s + 0x4BA8u));
            } else {
                if ((RD8(SNDB + (uint32_t)a6s + 0x4BA8u) & 1u) == 0u) v12 += 6u;
                v21 = RD8(0x8005A7E4u + RD8(SNDB + (uint32_t)a6s + 0x4BA8u));
            }
            sum = (int32_t)(int8_t)v21 + (int32_t)(RD8(p + 4u) & 0x7Fu);
            t1 = (uint32_t)sum;
            t0 = t1;
            if (((uint32_t)sum << 16) & 0x80000000u) {
                t1 = 0u;
                t0 = 0u;
            }
            if (RD8(SNDB + (uint32_t)a6s + 0x4BA8u) == 0u) WR8(SNDB + (uint32_t)a6s + 0x4BB4u, 0u);
        }
        b2 = RD8(p + 2u);
        if (b2 == 255u) {
            x = fp + 192u;
            xi = (int32_t)(int16_t)x;
            if (xi == 0) {
                v0 = 0u;
            } else {
                /* mult by 0x2AAAAAAB, high word, minus the sign of xi */
                pr = (int32_t)(((int64_t)xi * 715827883LL) >> 32);
                v0 = (uint32_t)pr - (xi < 0 ? 0xFFFFFFFFu : 0u);
            }
        } else {
            v0 = b2;
        }
        ax = (int32_t)(int16_t)v0;
        if (ax < 64) {
            pr = (int32_t)(int16_t)t1 * ax;
            t1 = (uint32_t)pr >> 6;
        } else {
            pr = (127 - ax) * (int32_t)(int16_t)t0;
            t0 = (uint32_t)pr >> 6;
        }
        flag = RD8(SNDB + 0x4BB9u);
        if (flag == 0u) {
            vol = (int32_t)(int8_t)RD8(0x8006CE59u);
            pa = (int32_t)(int16_t)t1 * vol;
            vp = (uint32_t)pa;
            if (pa < 0) vp += 127u;
            pb = (int32_t)(int16_t)t0 * vol;
            vq = (uint32_t)pb;
            if (pb < 0) vq += 127u;
            t1 = vp >> 7;
            t0 = vq >> 7;
        }
        WR32(sp + 0x10u, t2);
        WR32(sp + 0x14u, 0u);
        WR32(sp + 0x18u, (uint32_t)(int32_t)(int16_t)t0);
        WR32(sp + 0x1Cu, (uint32_t)(int32_t)(int16_t)t1);
        v0 = m->call4(m->ctx, 0x8003FA90u, (uint32_t)(int32_t)(int16_t)v12,
                      (uint32_t)(int32_t)(int16_t)a1x, a2b, a3b);
        break;

    case 8:
        v25 = (RD32(p + 4u) >> 7) & 0x1Fu;
        fl = RD8(SNDB + v25 + 0x4B84u);
        if ((RD32(p) & 0xFFFu) == 1u) {
            WR8(SNDB + v25 + 0x4B84u, fl ^ 1u);
            s0 = RD32(0x8005A7D0u + (a1 << 2)) + (RD16(p + 4u) << 3) + (fl != 0u ? 128u : 0u);
        } else {
            s0 = RD32(0x8005A7D0u + (a1 << 2)) + (RD16(p + 4u) << 3);
        }
        do {
            WR32(sp + 0x10u, (uint32_t)(int32_t)(int16_t)fp);
            WR32(sp + 0x14u, (uint32_t)(int32_t)(int8_t)s6);
            man_80023C48(m, s0, a1, a2, a3 & 0xFFFFu);
            bit = (RD32(s0 + 4u) >> 23) & 1u;
            s0 += 8u;
        } while (bit != 0u);
        v0 = bit;
        break;

    case 9:
        vp = RD32(p) & 0xFFFu;
        b2 = RD8(p + 2u);
        if (vp == 4095u) {
            v0 = m->call4(m->ctx, 0x8003E130u, b2, b2, a2, a3);
        } else {
            v0 = m->call4(m->ctx, 0x8003FFACu, vp, b2, b2, a3);
        }
        break;

    case 11:
        vp = RD32(p) & 0xFFFu;
        if (vp == 4095u) {
            v0 = m->call4(m->ctx, 0x8003F940u, 0u, a1, a2, a3);
            break;
        }
        ix = (a2 != 0u) ? vp + 10u : vp;
        addr = SNDB + (uint32_t)(int32_t)(int16_t)ix;
        b2 = RD8(p + 2u);
        vq = RD8(addr + 0x4B5Cu);
        if (b2 < vq) {
            v0 = 1u;
            break;
        }
        WR8(addr + 0x4B5Cu, 0u);
        v0 = m->call4(m->ctx, 0x8003FDECu, (uint32_t)(int32_t)(int16_t)ix, addr, a2, a3);
        break;

    case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 10:
        /* jump-table entries point at the epilogue: $v0 still holds the table entry */
        v0 = 0x8002409Cu;
        break;

    default:
        /* 12..15: sltiu left $v0 = 0 */
        v0 = 0u;
        break;
    }

done:
    m->sp_set(m->ctx, sp + 0x50u);
    return v0;
}
