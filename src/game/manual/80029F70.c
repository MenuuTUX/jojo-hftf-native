#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_8002A8A8(const GuestMem *m);

/* Index of the 8-byte table row used by the 8002A6F4 / 8002A7B8 / 8002A830 helpers: byte at 0x800549A8 + byte(0x8006D2D1). */
static uint32_t tbl_off(const GuestMem *m) {
    return (uint32_t)RD8(0x800549A8u + (uint32_t)RD8(0x8006D2D1u)) * 8u;
}

/* 8002A6F4 (called with a0 = x, a1 = y). Compares are signed (slt) in the original. */
static void h6F4(const GuestMem *m, uint32_t x, uint32_t y) {
    uint32_t idx = tbl_off(m), t, v, a1 = y;
    int32_t v4;
    if (a1 == RD32(0x80066D60u)) {
        t = RD32(0x800549DCu + idx);
        v = RD32(0x80066CE8u);
        if (!((int32_t)t < (int32_t)v)) WR32(0x80066CE8u, t);
    } else {
        t = RD32(0x800549E0u + idx);
        v = RD32(0x80066CE8u);
        if (!((int32_t)v < (int32_t)t)) WR32(0x80066CE8u, t);
    }
    v4 = (int32_t)(x - a1) >> 1;
    if (RDS16(0x8005AFE4u) != 0) v4 = v4 >> 3;
    WR32(0x80066CE8u, RD32(0x80066CE8u) + (uint32_t)v4);
}

/* 8002A7B8 (called with a0 = x). */
static void h7B8(const GuestMem *m, uint32_t x) {
    uint32_t idx, t, v;
    int32_t a1 = (int32_t)(x + 0xFF400000u) >> 1;
    if (RDS16(0x8005AFE4u) != 0) a1 = a1 >> 3;
    idx = tbl_off(m);
    v = RD32(0x80066CE8u) + (uint32_t)a1;
    WR32(0x80066CE8u, v);
    t = RD32(0x800549E0u + idx);
    if (!((int32_t)v < (int32_t)t)) WR32(0x80066CE8u, t);
}

/* 8002A830 (called with a0 = x). */
static void h830(const GuestMem *m, uint32_t x) {
    uint32_t idx, t, v;
    int32_t a1 = (int32_t)(0xC00000u - x) >> 1;
    if (RDS16(0x8005AFE4u) != 0) a1 = a1 >> 3;
    idx = tbl_off(m);
    t = RD32(0x800549DCu + idx);
    v = RD32(0x80066CE8u) - (uint32_t)a1;
    WR32(0x80066CE8u, v);
    if (!((int32_t)t < (int32_t)v)) WR32(0x80066CE8u, t);
}

/* 80029F70: per-frame update of the struct at a0 (fields 0x10, 0x14, 0x16, 0x46, 0x74, 0x78, 0x84, 0x88, 0x8C) against the
 * 0x80066CD8 block. Result: 0x80066D70, or the return value of 8002A8A8 when 0x8005AFE8 is nonzero. */
uint32_t man_80029F70(const GuestMem *m, uint32_t a0) {
    uint32_t s0 = a0, a1 = 0, a2 = 0, a3 = 0, v0 = 0, v1 = 0, t = 0, idx = 0;

    a0 = 0;
    if (RD16(0x8005AFECu) != 0) goto L3a4;
    if (RD8(0x8008EBC9u) != 0) goto L168;
    if (RD8(0x800CDF54u) == 1u) a0 = RD16(0x800CDDBEu);
    if (RD8(0x800CDF55u) == 1u && RD8(0x800CD46Cu) == 1u && (int32_t)(int16_t)a0 < RDS16(0x800CD2D6u))
        a0 = RD16(0x800CD2D6u);
    if (RD8(0x800CE2E4u) == 1u && (int32_t)(int16_t)a0 < RDS16(0x800CE14Eu))
        a0 = RD16(0x800CE14Eu);
    if (RD8(0x800CE2E5u) == 1u && RD8(0x800CD7FCu) == 1u && (int32_t)(int16_t)a0 < RDS16(0x800CD666u))
        a0 = RD16(0x800CD666u);
    WR32(s0 + 0x14u, RD8(0x8006D2D1u) == 31u ? 0x013C0000u : 0u);
    if (RDS16(0x8005AFFAu) != 0) goto L168;

    if ((int32_t)(int16_t)a0 >= 65) {
        WR16(s0 + 0x84u, a0);
        a0 = (uint32_t)((int32_t)(int16_t)(a0 - 64u) >> 2);
    } else if (RDS16(s0 + 0x84u) >= 65) {
        v1 = RD16(s0 + 0x84u);
        a0 = (uint32_t)((int32_t)(int16_t)(v1 - 64u) >> 2);
        WR16(s0 + 0x84u, (uint16_t)(v1 - a0));
    } else {
        WR16(s0 + 0x84u, 0);
        goto L164;
    }
    v1 = RD16(s0 + 0x46u);
    if (v1 == 48u) {
        if ((int32_t)(int16_t)a0 >= 30) a0 = 29u;
    } else {
        if ((int32_t)(int16_t)a0 >= 14) a0 = 13u;
    }
    WR16(s0 + 0x16u, (uint16_t)(RD16(s0 + 0x16u) + a0));

L164:
L168:
    if (RD8(0x8008EBC8u) != 0) goto L690;
    if (RD8(0x800CDF54u) == 0) goto L1ac;
    if (RD16(0x8005AFEAu) == 2u) goto L1ac;
    a3 = 1;
    a0 = RD32(0x800CDDB8u);
    goto L1d4;

L1ac:
    v0 = (RD16(0x8005AFE6u) == 0) ? 0xC00000u : 0x800000u;
    a0 = RD32(s0 + 0x10u) + v0;

L1d4:
    a2 = a0;
    if (RD8(0x800CD46Cu) == 0) goto L238;
    if (RD16(0x8005AFEAu) == 2u) goto L238;
    if (a3 == 0) {
        a0 = RD32(0x800CD2D0u);
        a3 = 1;
        a2 = a0;
        goto L238;
    }
    v1 = RD32(0x800CD2D0u);
    if ((int32_t)v1 < (int32_t)a2) {
        a2 = v1;
        goto L238;
    }
    a0 = v1;

L238:
    if (RD8(0x800CE2E4u) == 0) goto L2a0;
    if (RD16(0x8005AFEAu) == 1u) goto L2a0;
    if (a3 == 0) {
        a0 = RD32(0x800CE148u);
        a3 = 1;
        a2 = a0;
        goto L2a0;
    }
    v1 = RD32(0x800CE148u);
    if ((int32_t)v1 < (int32_t)a2) {
        a2 = v1;
        goto L2a0;
    }
    if ((int32_t)a0 < (int32_t)v1) a0 = v1;

L2a0:
    if (RD8(0x800CD7FCu) == 0) goto L308;
    if (RD16(0x8005AFEAu) == 1u) goto L308;
    if (a3 == 0) {
        a0 = RD32(0x800CD660u);
        a3 = 1;
        a2 = a0;
        goto L308;
    }
    v1 = RD32(0x800CD660u);
    if ((int32_t)v1 < (int32_t)a2) {
        a2 = v1;
        goto L308;
    }
    if ((int32_t)a0 < (int32_t)v1) a0 = v1;

L308:
    if (RD16(0x8005AFF4u) == 0) goto L384;
    if (RD16(0x8005AFF6u) != 0) goto L384;
    if (RD16(0x8005AFE6u) != 0) goto L384;
    v1 = RD16(0x8005AFF8u);
    if (v1 == 0u) {
        a0 = RD32(0x800CDDB8u);
    } else if (v1 == 1u) {
        a0 = RD32(0x800CE148u);
    } else {
        a0 = RD32(s0 + 0x10u) + 0xC00000u;
    }
    a2 = a0;

L384:
    v1 = RD32(s0 + 0x10u);
    a1 = v1 & 0xFFFF0000u;
    a2 = (a2 - a1) & 0xFFFF0000u;
    a0 = (a0 - a1) & 0xFFFF0000u;
    goto L3f4;

L3a4:
    a3 = 1;
    a2 = (RD16(0x8005AFE6u) == 0) ? 0xC00000u : 0x800000u;
    v1 = RD32(0x8005AFF0u);
    a0 = a2;
    if ((int32_t)v1 > 0) {
        a0 = RD32(s0 + 0x8cu) + v1;
        a2 = a0;
    } else if ((int32_t)v1 < 0) {
        a0 = RD32(s0 + 0x88u) + v1;
        a2 = a0;
    }

L3f4:
    if (a3 == 0) goto L690;
    if (RD16(0x8005AFE2u) != 0) goto L690;
    a1 = RD32(s0 + 0x88u);
    if ((int32_t)a1 < (int32_t)a2) goto L630;
    if ((int32_t)a1 < (int32_t)a0) goto L4b4;
    if ((int32_t)a2 < (int32_t)a0) {
        h6F4(m, a2, a1);
    } else {
        h6F4(m, a0, a1);
    }
    /* table block (MIPS 43c..470 and 47c..4b0): */
    idx = tbl_off(m);
    t = RD32(0x800549DCu + idx);
    if (!((int32_t)t < (int32_t)RD32(s0 + 0x10u))) WR32(s0 + 0x10u, t);
    goto L690;

L4b4:
    if (!((int32_t)RD32(s0 + 0x8cu) < (int32_t)a0)) goto L5f0;
    a1 = (((uint32_t)((int32_t)(a0 - a2) >> 1)) + a2) & 0xFFFF0000u;
    if ((int32_t)0x00BFFFFFu < (int32_t)a1) goto L564; /* lui 0x00BF; ori 0xFFFF */
    /* MIPS 4f0..52c */
    idx = tbl_off(m);
    t = RD32(0x800549DCu + idx);
    if (!((int32_t)t < (int32_t)RD32(s0 + 0x10u))) {
        WR32(s0 + 0x10u, t);
        goto L690;
    }
    h830(m, a1);
    /* MIPS 53c..5ec: table re-read after the call, same test */
    idx = tbl_off(m);
    t = RD32(0x800549DCu + idx);
    if (!((int32_t)t < (int32_t)RD32(s0 + 0x10u))) WR32(s0 + 0x10u, t);
    goto L690;

L564:
    if (!((int32_t)0xC00000u < (int32_t)a1)) goto L690;
    /* MIPS 56c..5b0 */
    idx = tbl_off(m);
    t = RD32(0x800549E0u + idx);
    if (!((int32_t)RD32(s0 + 0x10u) < (int32_t)t)) {
        WR32(s0 + 0x10u, t);
        goto L690;
    }
    h7B8(m, a1);
    /* MIPS 5b8..5ec */
    idx = tbl_off(m);
    t = RD32(0x800549E0u + idx);
    if (!((int32_t)RD32(s0 + 0x10u) < (int32_t)t)) WR32(s0 + 0x10u, t);
    goto L690;

L5f0:
    h6F4(m, a2, a1);
    idx = tbl_off(m);
    t = RD32(0x800549DCu + idx);
    if (!((int32_t)t < (int32_t)RD32(s0 + 0x10u))) WR32(s0 + 0x10u, t);
    goto L690;

L630:
    a1 = RD32(s0 + 0x8cu);
    if (!((int32_t)a1 < (int32_t)a0)) goto L690;
    h6F4(m, a0, a1);
    idx = tbl_off(m);
    t = RD32(0x800549E0u + idx);
    if (!((int32_t)RD32(s0 + 0x10u) < (int32_t)t)) WR32(s0 + 0x10u, t);
    goto L690;

L690:
    WR32(s0 + 0x74u, RD32(s0 + 0x10u));
    WR32(s0 + 0x78u, RD32(s0 + 0x14u));
    v1 = RD16(0x8005AFE8u);
    WR32(0x80066DE4u, RD32(0x80066D80u));
    WR32(0x80066DE8u, RD32(0x80066D84u));
    if (v1 != 0u) return man_8002A8A8(m);
    return 0x80066D70u;
}
