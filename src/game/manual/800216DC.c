#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_80020C60(const GuestMem *m, uint32_t a0);
uint32_t man_800227A8(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2);

/* 800216DC: builds the GP0 primitive(s) for one object record. a0 = object (s0), a1 = output cursor (s1),
 * a2 = record (s2), a3 is passed through unchanged to 800209AC. Frame: 0x58 bytes; the byte buffer at sp+0x10..0x1B
 * and the spill words at sp+0x20/0x24/0x28 live in guest memory as in the original. Control flow follows the MIPS
 * labels (L_<addr>) and every delay-slot effect is placed where it executes. Returns the advanced output cursor, or 0
 * when out + 40 is not below the limit 0x80085BD8 + (word at 0x80066E14 << 14) (checked at each inner step, 80021898). */
uint32_t man_800216DC(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t s0 = a0;
    uint32_t s1 = a1;
    uint32_t s2 = a2;
    uint32_t s3 = 0u;
    uint32_t s4 = 0u;
    uint32_t s5 = 0u;
    uint32_t s6 = 0u;
    uint32_t s7 = 0u;
    uint32_t fp = 0u;
    uint32_t v0 = 0u, v1 = 0u;
    uint32_t t0 = 0u, t1 = 0u, t2 = 0u, t3 = 0u;
    uint32_t ta0 = 0u, ta1 = 0u, ta2 = 0u, ta3 = 0u;
    uint32_t mulk = 0u;
    uint32_t r = 0u;
    uint32_t ret = 0u;
    uint32_t sp;
    uint64_t prod = 0u;

    sp = m->sp_get(m->ctx) - 0x58u;     /* addiu sp,sp,-88 */
    m->sp_set(m->ctx, sp);

    if ((RD16(s2 + 0x10u) & 0x8000u) != 0u) {
        ret = s1;
        goto done;
    }

    /* 80021724..8002172c: jal 800209ac with a0 = object, a1 = record; a2 and a3 are still the entry values */
    m->call4(m->ctx, 0x800209ACu, s0, s2, a2, a3);

    WR32(s0 + 0x0Cu, s1);
    WR32(s0 + 0x08u, s1);
    v0 = RD32(s2 + 0xA0u);
    v1 = RD16(s0 + 0x2Cu);
    WR32(s0 + 0x14u, v0);
    v0 = (v1 * 3u) << 2;
    ta0 = RD32(s2 + 0xA4u);
    s7 = ta0 + v0;
    WR32(s0 + 0x18u, ta0);
    WR32(s0 + 0x28u, s7);

    v0 = RD8(s2 + 0x0Bu);
    ta0 = s0;
    WR8(s0 + 0x3Du, v0);
    v1 = RD8(s2 + 0x12u);
    v0 = v0 & 1u;
    ta1 = v1 & 1u;
    v0 = v0 ^ ta1;
    v0 = v0 << 1;
    WR8(s0 + 0x8Fu, v1);
    v1 = v1 & 2u;
    v1 = v1 >> 1;
    v0 = v0 + v1;
    WR8(s0 + 0x3Eu, v0);        /* sb in the delay slot of the jal below */
    s5 = man_80020C60(m, ta0);
    if (s5 == 0u) {
        ret = s1;
        goto done;
    }

    ta0 = s0;                   /* addu a0,s0 in the delay slot of the beq */
    m->call4(m->ctx, 0x80022674u, s0, s2, 0u, 0u);   /* a1 = s2 in the delay slot; a2/a3 not read by the callee */

    v0 = RD16(s7 + 4u);
    ta1 = RD16(s0 + 0x42u);
    WR16(s0 + 0x32u, v0);
    v0 = RD16(s7 + 6u);
    WR16(s0 + 0x34u, v0);
    v0 = 0xAAAAAAABu;
    prod = (uint64_t)ta1 * (uint64_t)v0;    /* multu a1,v0 */
    s6 = 0u;
    v0 = RD16(s7 + 0u);
    ta3 = RD32(s0 + 0x18u);
    v0 = v0 << 2;
    ta3 = ta3 + v0;
    WR32(s0 + 0x24u, ta3);
    t0 = RD8(s7 + 2u);
    v1 = RD8(s7 + 3u);
    ta2 = 0x8006CEE0u;          /* lui a2,0x8007; addiu a2,a2,-12576 */
    WR8(s0 + 0x3Bu, 0u);
    t1 = (uint32_t)(prod >> 32);    /* mfhi t1 */
    ta0 = t1 >> 4;
    v0 = ta0 * 24u;             /* sll/addu/sll: 24 * a0 */
    ta1 = ta1 - v0;
    ta1 = ta1 & 0xFFFFu;
    ta0 = ta0 & 0xFFFFu;
    mulk = t0 * v1;             /* mult t0,v1 (mflo t0 comes later) */
    v1 = RD16(s0 + 0x90u);
    ta0 = ta0 - 480u;
    v1 = v1 + ta0;
    v0 = v1 * 24u;              /* sll/addu/sll: 24 * v1 */
    ta1 = ta1 + v0;
    ta1 = ta1 + ta2;
    v0 = RD8(ta1);
    fp = s6;                    /* addu fp,s6,zero */
    WR8(s0 + 0x3Eu, v0);
    v1 = RD8(s7 + 2u);
    t0 = mulk;                  /* mflo t0 */
    v0 = t0 + 31u;
    v0 = (uint32_t)((int32_t)v0 >> 5);
    v0 = v0 << 2;
    s4 = ta3 + v0;              /* addu s4,a3,v0 (delay slot of the beq below) */
    if (v1 == 0u) {
        goto L_80021C84;
    }
    WR32(sp + 0x28u, 0u);       /* sw zero,0x28(sp) */

L_8002186C:
    v0 = RD8(s7 + 3u);
    s3 = 0u;                    /* addu s3,zero,zero (delay slot of the beq: runs on both paths) */
    if (v0 == 0u) {
        goto L_80021C60;
    }
    WR32(sp + 0x20u, fp << 4);
    WR32(sp + 0x24u, RD32(sp + 0x28u));

L_80021894:
    v1 = RD32(0x80066E14u);
    ta0 = s1 + 40u;
    v0 = 0x80085BD8u;
    v1 = (v1 << 14) + v0;
    ta0 = (ta0 < v1) ? 1u : 0u;     /* sltu a0,a0,v1 */
    t2 = 8u;                    /* addiu t2,zero,8 (delay slot of the beq: runs on both paths) */
    if (ta0 == 0u) {
        goto L_80021CF8;
    }
    if (s6 == 8u) {             /* bne s6,t2 falls through only when equal */
        s6 = 0u;
        s5 = RD32(s5 + 8u);
    }

    v0 = RD8(s0 + 0x3Bu);
    if (v0 != 0u) {
        goto L_800218F4;
    }
    v0 = RD32(s0 + 0x24u);
    t2 = 8u;
    WR8(s0 + 0x3Bu, t2);
    v1 = RD8(v0);
    v0 = v0 + 1u;
    WR32(s0 + 0x24u, v0);
    WR8(s0 + 0x3Au, v1);

L_800218F4:
    v0 = RD8(s0 + 0x3Au) & 0x80u;
    if (v0 == 0u) {
        goto L_80021C34;
    }
    v1 = RD8(s0 + 0x3Du) & 1u;
    v0 = RD8(s0 + 0x8Fu) & 1u;
    if (v1 == v0) {
        goto L_80021938;
    }
    v0 = RD16(s0 + 0x32u);
    t2 = RD32(sp + 0x24u);
    v0 = v0 + t2;
    ta3 = v0 - 16u;             /* addiu a3,v0,-16 (delay slot of the j) */
    goto L_80021948;

L_80021938:
    v0 = RD16(s0 + 0x32u);
    t2 = RD32(sp + 0x20u);
    ta3 = t2 - v0;              /* subu a3,t2,v0 */

L_80021948:
    v0 = ta3 + 16u;
    WR16(s2 + 0xD4u, ta3);
    WR16(s2 + 0xD0u, ta3);
    WR16(s2 + 0xD6u, v0);
    WR16(s2 + 0xD2u, v0);

    if ((RD8(s0 + 0x8Fu) & 2u) == 0u) {
        v0 = s3 << 4;           /* sll in the delay slot of the beq */
        goto L_8002197C;
    }
    v0 = s3 << 4;
    v1 = RD16(s0 + 0x34u);
    ta3 = v0 - v1;              /* subu a3,v0,v1 (delay slot of the j) */
    goto L_8002198C;

L_8002197C:
    v0 = RD16(s0 + 0x34u);
    v1 = s3 << 4;
    v0 = v0 - v1;
    ta3 = v0 - 16u;             /* addiu a3,v0,-16 */

L_8002198C:
    v0 = ta3 + 16u;
    WR16(s2 + 0xDAu, v0);
    WR16(s2 + 0xD8u, v0);
    WR16(s2 + 0xDEu, ta3);
    WR16(s2 + 0xDCu, ta3);      /* sh in the delay slot of the jal */
    man_800227A8(m, s0, s2, 4u);

    r = m->call4(m->ctx, 0x8002344Cu, s0, 4u, 0u, 0u);     /* a0 = s0, a1 = 4 (delay slot); a2/a3 not read */
    v0 = 0x09000000u;           /* lui v0,0x900 (delay slot of the bne) */
    if (r != 0u) {
        goto L_80021C2C;
    }
    WR32(s1, v0);               /* sw v0,0(s1) */

    v0 = RD8(s5);
    v1 = s6 << 4;
    v0 = v0 & 0x1Eu;
    v0 = v0 << 3;
    WR8(sp + 0x12u, v0);
    WR8(sp + 0x10u, v0);
    v0 = v0 + 15u;
    WR8(sp + 0x13u, v0);
    WR8(sp + 0x11u, v0);
    v0 = RD8(s5);
    t0 = s1;
    v0 = v0 & 1u;
    v0 = v0 << 7;
    v0 = v0 + v1;
    WR8(sp + 0x19u, v0);
    WR8(sp + 0x18u, v0);
    v0 = v0 + 15u;
    WR8(sp + 0x1Bu, v0);
    WR8(sp + 0x1Au, v0);

    v0 = (uint32_t)(int32_t)RDS8(s0 + 0x48u);
    s1 = s1 + 4u;               /* addiu s1,s1,4 (delay slot of the bgez) */
    if ((int32_t)v0 > 0) {
        ta1 = RD16(s0 + 0x42u);
        goto L_80021A8C;
    }
    /* lb 0x48 is zero or negative: the 135 test on 0x3c */
    if (RD8(s0 + 0x3Cu) == 135u) {
        v0 = RD8(s4 + 3u) + RD16(s0 + 0x42u);
    } else {
        v0 = RD16(s0 + 0x42u) + (RD8(s4 + 3u) & 0x3Fu);
    }
    ta1 = v0 & 0xFFFFu;

L_80021A8C:
    prod = (uint64_t)ta1 * (uint64_t)0xAAAAAAABu;   /* multu a1 */
    ta3 = 0u;
    v1 = RD16(s0 + 0x90u);
    t2 = (uint32_t)(prod >> 32);    /* mfhi t2 */
    ta0 = t2 >> 4;
    v1 = v1 + ta0;
    v1 = v1 << 6;
    v0 = ta0 * 24u;
    ta1 = ta1 - v0;
    ta1 = ta1 & 0x3Fu;
    v1 = v1 | ta1;
    t1 = v1 << 16;

L_80021ACC:
    v0 = ta3 << 16;
    ta2 = (uint32_t)((int32_t)v0 >> 16);
    v0 = ta2 << 1;              /* sll in the delay slot of the bne */
    if (ta2 != 0u) {
        goto L_80021B40;
    }
    v0 = RD8(s2 + 0xE0u);
    v1 = RD8(s0 + 0x3Eu);
    ta0 = v0 * v1;              /* mult v0,v1; mflo a0 */
    v0 = RD8(s2 + 0xE4u);
    t3 = v0 * v1;               /* mflo t3 */
    v0 = RD8(s2 + 0xE8u);
    ta1 = v0 * v1;              /* mflo a1 */
    v1 = ta0 >> 7;
    v0 = t3 >> 7;
    v0 = v0 << 8;
    v1 = v1 | v0;
    v0 = ta1 >> 7;
    v0 = v0 << 16;
    v1 = v1 | v0;
    v0 = 0x2C000000u;
    v1 = v1 | v0;
    WR32(s1, v1);
    s1 = s1 + 4u;
    v0 = ta2 << 1;              /* sll v0,a2,1 (i == 0 gives 0) */

L_80021B40:
    v0 = s0 + v0;
    v1 = (uint32_t)(int32_t)RDS16(v0 + 0x80u);
    v0 = RD16(v0 + 0x78u);
    v1 = v1 << 16;
    v0 = v0 | v1;
    WR32(s1, v0);
    s1 = s1 + 4u;               /* addiu s1,s1,4 (delay slot of the bne: runs on both paths) */
    if (ta2 != 0u) {
        goto L_80021B7C;
    }
    v1 = RD8(sp + 0x18u);
    v0 = RD8(sp + 0x10u);
    v1 = v1 << 8;
    v0 = v0 | v1;
    v0 = v0 | t1;
    WR32(s1, v0);               /* sw in the delay slot of the j */
    goto L_80021BB0;

L_80021B7C:
    v1 = sp + ta2;
    v0 = RD8(v1 + 0x18u);
    ta0 = RD8(v1 + 0x10u);
    v0 = v0 << 8;
    ta0 = ta0 | v0;
    v0 = RD16(s5);
    v1 = RD8(s0 + 0x41u);
    v0 = v0 >> 5;
    v0 = v0 + v1;
    v0 = v0 << 16;
    ta0 = ta0 | v0;
    WR32(s1, ta0);

L_80021BB0:
    v0 = ta3 + 1u;
    ta3 = v0;
    v0 = v0 << 16;
    v0 = (uint32_t)((int32_t)v0 >> 16);
    v0 = ((int32_t)v0 < 4) ? 1u : 0u;
    s1 = s1 + 4u;               /* addiu s1,s1,4 (delay slot of the bne) */
    if (v0 != 0u) {
        goto L_80021ACC;
    }

    v0 = RD8(s0 + 0x40u);
    if (v0 == 0u) {
        goto L_80021BE8;
    }
    v0 = RD8(t0 + 7u) | 2u;     /* ori in the delay slot of the j */
    goto L_80021BF4;

L_80021BE8:
    v0 = RD8(t0 + 7u) & 0xFDu;

L_80021BF4:
    WR8(t0 + 7u, v0);
    v0 = RD32(s0 + 8u);
    if (v0 == t0) {
        goto L_80021C2C;
    }
    ta0 = 0x00FFFFFFu;
    ta1 = RD32(s0 + 0xCu);
    v1 = 0xFF000000u;
    v0 = RD32(ta1);
    ta0 = t0 & ta0;
    v0 = v0 & v1;
    v0 = v0 | ta0;
    WR32(ta1, v0);
    WR32(s0 + 0xCu, t0);

L_80021C2C:
    s6 = s6 + 1u;
    s4 = s4 + 4u;

L_80021C34:
    v0 = RD8(s0 + 0x3Au);
    v1 = RD8(s0 + 0x3Bu);
    v0 = v0 << 1;
    v1 = v1 - 1u;
    WR8(s0 + 0x3Au, v0);
    WR8(s0 + 0x3Bu, v1);
    v0 = RD8(s7 + 3u);
    s3 = s3 + 1u;
    v0 = ((int32_t)s3 < (int32_t)v0) ? 1u : 0u;
    if (v0 != 0u) {
        goto L_80021894;
    }

L_80021C60:
    WR32(sp + 0x28u, RD32(sp + 0x28u) - 16u);
    v0 = RD8(s7 + 2u);
    fp = fp + 1u;
    v0 = ((int32_t)fp < (int32_t)v0) ? 1u : 0u;
    if (v0 != 0u) {
        goto L_8002186C;
    }

L_80021C84:
    ta0 = RD32(s0 + 8u);
    ta1 = RD32(s0 + 0xCu);
    if (ta0 == ta1) {
        goto L_80021D00;
    }
    ta2 = 0x00FFFFFFu;
    ta3 = 0xFF000000u;
    v0 = RD8(s2 + 0x32u);
    ta0 = RD32(0x80066E10u);
    v1 = RD32(ta1);
    v0 = v0 << 2;
    v0 = v0 + ta0;
    v0 = RD32(v0);
    v1 = v1 & ta3;
    v0 = v0 & ta2;
    v1 = v1 | v0;
    v0 = s1;
    WR32(ta1, v1);
    ta1 = RD8(s2 + 0x32u);
    v1 = RD32(s0 + 8u);
    ta1 = ta1 << 2;
    ta1 = ta1 + ta0;
    ta0 = RD32(ta1);
    v1 = v1 & ta2;
    ta0 = ta0 & ta3;
    ta0 = ta0 | v1;
    WR32(ta1, ta0);
    ret = v0;
    goto done;

L_80021CF8:
    ret = 0u;
    goto done;

L_80021D00:
    if (ta0 == s1) {
        ret = s1;
        goto done;
    }
    ta1 = 0x00FFFFFFu;
    ta3 = 0xFF000000u;
    v0 = RD8(s2 + 0x32u);
    ta2 = RD32(0x80066E10u);
    v1 = RD32(ta0);
    v0 = v0 << 2;
    v0 = v0 + ta2;
    v0 = RD32(v0);
    v1 = v1 & ta3;
    v0 = v0 & ta1;
    v1 = v1 | v0;
    WR32(ta0, v1);
    ta0 = RD8(s2 + 0x32u);
    v0 = RD32(s0 + 8u);
    ta0 = ta0 << 2;
    ta0 = ta0 + ta2;
    v1 = RD32(ta0);
    v0 = v0 & ta1;
    v1 = v1 & ta3;
    v1 = v1 | v0;
    WR32(ta0, v1);
    ret = s1;

done:
    m->sp_set(m->ctx, sp + 0x58u);
    return ret;
}
