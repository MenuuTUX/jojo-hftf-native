#include "manual.h"
#include "hex2c_rt.h"

/* 800AD2A4 (AC_MAIN). $a0 = A (struct), $a1 = B (struct), $a2 = P (word pointer, read and written by this function).
 * $a3 is never written here, so every call passes it on unchanged.
 * Calls that do not set $a1 themselves pass the $a1 left by the previous call (aa1): 8003D3C0 (x3) and 8003D650.
 * Those callees take one argument and do not read $a1, so the value only keeps the register picture faithful.
 * Every path ends at 800AD5C0: the return value is the $v0 from that shift (or 0x3F800000). */
uint32_t man_800AD2A4(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t s0 = RD32(a2);                 /* 800AD2F4 */
    uint32_t s3 = RD32(a1 + 0x174u);        /* 800AD2F8 */
    uint32_t fp = RD32(a1 + 0x17Cu);        /* 800AD2FC */
    uint32_t aa1 = 0u;                      /* $a1 = $s1 = 0 at 800AD2C8 */
    uint32_t s1 = 0u;
    uint32_t v0, v1, t, r, hb, ix, s1old;
    int32_t hv, d;

    v0 = m->call4(m->ctx, 0x8003D1D0u, s0, aa1, a2, a3);
    if (v0 == 0u)
        return 0u;

    /* 800AD310: first block, enabled by byte at 0x157 of B */
    if (RD8(s3 + 0x157u) != 0u) {
        t = RD32(s3 + 0x130u);
        v1 = RD8(t + 0x27u);
        aa1 = ((int32_t)v1 < 10) ? RD32(0x800CB0F8u + 4u * v1) : RD32(0x800CB120u);
        v0 = m->call4(m->ctx, 0x8003D720u, s0, aa1, a2, a3);
        WR32(a2, v0);
        v0 = m->call4(m->ctx, 0x8003D3C0u, RD32(a2), aa1, a2, a3);
        v0 <<= 16;
        if ((int32_t)v0 <= 0) {
            WR32(a2, 0x3F800000u);
            v0 = 0x3F800000u;
        }
    }

    /* 800AD388: unless byte 0x80066C6C is 0 or 5 */
    v1 = (uint32_t)RDS8(0x80066C6Cu);
    if (v1 != 0u && v1 != 5u) {
        ix = (uint32_t)RDS8(0x8006CE55u);
        aa1 = RD32(0x800CB128u + 4u * ix);
        v0 = m->call4(m->ctx, 0x8003D720u, RD32(a2), aa1, a2, a3);
        WR32(a2, v0);
    }

    /* 800AD3C8: unless byte 0x80066CA3 is 0 or equal to (signed) byte 0x136 of A vs byte 0x80066CA4 */
    if (RD8(0x80066CA3u) != 0u && (uint32_t)RDS8(a0 + 0x136u) != RD8(0x80066CA4u)) {
        ix = (uint32_t)RDS8(0x8006D2F0u);
        aa1 = RD32(0x800CB138u + 4u * ix);
        r = m->call4(m->ctx, 0x8003D720u, RD32(a2), aa1, a2, a3);
        WR32(a2, r);
        v0 = m->call4(m->ctx, 0x8003D3C0u, r, aa1, a2, a3);
        v0 <<= 16;
        if ((int32_t)v0 <= 0) {
            WR32(a2, 0x3F800000u);
            v0 = 0x3F800000u;
        }
    }

    /* 800AD430: index from halfword 0x16A of B, rounded up when negative */
    hv = (int32_t)RDS16(s3 + 0x16Au);
    ix = (uint32_t)((hv >= 0) ? (hv >> 4) : ((hv + 15) >> 4));
    aa1 = RD32(0x800CB178u + 4u * ix);
    r = m->call4(m->ctx, 0x8003D720u, RD32(a2), aa1, a2, a3);
    WR32(a2, r);
    ix = 0x800CB1A4u + 4u * RD8(a1 + 0x13Au) + 8u * (uint32_t)RDS8(a1 + 0x137u);
    aa1 = RD32(ix);
    s1 = m->call4(m->ctx, 0x8003D020u, 0u, aa1, a2, a3);

    /* 800AD498: d = (0x16A of B - 0x16A of the FP record) >> 4 */
    d = (int32_t)RDS16(s3 + 0x16Au) - (int32_t)RDS16(fp + 0x16Au);
    d >>= 4;
    if (d > 0) {
        r = m->call4(m->ctx, 0x8003D650u, (uint32_t)d << 1, aa1, a2, a3);
        s1 = m->call4(m->ctx, 0x8003DDC0u, s1, r, a2, a3);
    }
    r = m->call4(m->ctx, 0x8003D720u, s1, RD32(a2), a2, a3);
    s1 = r;
    s1 = m->call4(m->ctx, 0x8003D720u, s1, 0x3D000000u, a2, a3);

    /* 800AD4EC: index from the table at 0x800CDD50 (byte 0x1FB of its records), then 0x13A of A */
    if (RD8(a0 + 0x1Au) == 2u) {
        hb = RD8(0x800CDD50u + 912u * (uint32_t)RDS8(a1 + 0x136u) + 0x1FBu);
        v1 = (hb == 2u) ? 3u : 1u;
    } else {
        hb = RD8(0x800CDD50u + 912u * (uint32_t)RDS8(a1 + 0x136u) + 0x1FBu);
        v1 = (hb == 2u) ? RD8(a0 + 0x13Au) + 2u : RD8(a0 + 0x13Au);
    }

    /* 800AD580 */
    ix = 0x800CB0D8u + 4u * RD8(a1 + 0x13Au) + 8u * v1;
    aa1 = RD32(ix);
    s1 = m->call4(m->ctx, 0x8003D720u, s1, aa1, a2, a3);
    s1old = s1;
    r = m->call4(m->ctx, 0x8003DDC0u, RD32(a2), s1, a2, a3);
    WR32(a2, r);
    v0 = m->call4(m->ctx, 0x8003D3C0u, r, s1old, a2, a3);
    v0 <<= 16;
    if ((int32_t)v0 <= 0) {
        WR32(a2, 0x3F800000u);
        v0 = 0x3F800000u;
    }
    return v0;
}
