#include "manual.h"
#include "hex2c_rt.h"

/* 800AAE9C has no frame of its own. Its fifth argument, the out pointer a5, is the caller's
   stack word at sp+0x10, read through sp_get because nothing has moved sp yet.
   Return values as the MIPS leaves $v0 at jr ra:
     a2 == 0 (signed 16-bit)  -> $v0 unchanged, i.e. in_v0
     a3 == 0 (signed 16-bit)  -> $v0 = a3 << 16 (which is 0 here)
     otherwise                -> $v0 = 1 (the delay slot of the final bne sets it on both paths)
   The table at 0x800CAFB0 holds per-struct pointer tables (lui 800D; addiu -20560). */
uint32_t man_800AAE9C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t in_v0) {
    const uint32_t tbl = 0x800CAFB0u;
    uint32_t a5 = RD32(m->sp_get(m->ctx) + 0x10u);
    uint32_t t6 = a0;
    uint32_t i = (uint32_t)(int32_t)(int16_t)(uint16_t)a2;
    uint32_t j = (uint32_t)(int32_t)(int16_t)(uint16_t)a3;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t t3;
    uint32_t t4;
    uint32_t v0;
    uint32_t v1;
    uint32_t x0;
    uint32_t x2;
    uint32_t x3;
    uint32_t y1;

    /* aaea4..aaeb0: *a5 = 0 (the sh sits in the beq delay slot, so it runs on every path) */
    WR16(a5, 0u);
    if (i == 0u) {
        return in_v0;
    }
    if (j == 0u) {
        return a3 << 16;
    }

    /* aaec0..aaee4: record pointers for struct A (a0) at +0x136 and index i */
    t0 = RD16(t6 + 0x6Au);
    v1 = RD32(tbl + ((uint32_t)RDS8(t6 + 0x136u) << 2));
    t4 = v1 + (i << 3);
    /* aaed0..aaf00: record pointers for struct B (a1) and index j */
    x3 = RD16(a1 + 0x6Au);
    v1 = RD32(tbl + ((uint32_t)RDS8(a1 + 0x136u) << 2));
    t3 = v1 + (j << 3);

    x2 = RD16(t4 + 0u);
    t2 = RD16(t4 + 2u);
    x0 = RD16(t3 + 0u);
    t1 = RD16(t3 + 2u);

    /* aaf0c..aaf28 */
    if (RD8(t6 + 0x0Bu) != 0u) {
        t0 = t0 + (~x2) - t2;
    } else {
        t0 = t0 + x2;
    }
    /* aaf2c..aaf48 */
    if (RD8(a1 + 0x0Bu) != 0u) {
        x3 = x3 + (~x0) - t1;
    } else {
        x3 = x3 + x0;
    }

    /* aaf4c..aaf68 */
    x2 = t0 + t2;
    x0 = x3 + t1;
    t1 = x0;
    if ((int32_t)(t0 << 16) < (int32_t)(x3 << 16)) {
        t0 = x3;
    }

    /* aaf6c..aaf88: the delay slot subu v0,a2,t0 runs first; if (t1 & 0xFFFF) < (a2 & 0xFFFF)
       then a2 = a0 and v0 is recomputed from the new a2 */
    v0 = x2 - t0;
    if ((t1 & 0xFFFFu) < (x2 & 0xFFFFu)) {
        x2 = x0;
        v0 = x2 - t0;
    }
    WR16(a5 + 2u, t0);
    WR16(a5 + 4u, x2);
    WR16(a5 + 0u, v0);

    /* aaf94..aafb4: the second record row (+4, +6) and the 0x6E fields */
    x2 = RD16(t4 + 4u);
    x0 = RD16(t3 + 4u);
    t2 = RD16(t4 + 6u);
    v0 = RD16(t6 + 0x6Eu);
    t1 = RD16(t3 + 6u);
    v1 = RD16(a1 + 0x6Eu);
    v0 = v0 + x2;
    t0 = v0;
    x2 = t2 + v0;
    x0 = v1 + x0;
    y1 = x0 + t1;

    /* aafc0..aafe8 */
    v0 = (t0 & 0xFFFFu) < (x0 & 0xFFFFu);
    t1 = y1;
    if (v0 != 0u) {
        t0 = x0;
    }
    v1 = x2 & 0xFFFFu;
    v0 = t1 & 0xFFFFu;
    if (v0 < v1) {
        x2 = y1;
    }

    /* aaff0..ab018 */
    v0 = RD16(a5 + 0u);
    v1 = x2 - t0;
    WR16(a5 + 6u, t0);
    WR16(a5 + 8u, x2);
    v0 = v0 + v1;
    WR16(a5 + 0u, v0);
    v0 = v0 & 0xFFFFu;
    if (v0 == 0u) {
        WR16(a5 + 0u, 1u);
    }
    return 1u;
}
