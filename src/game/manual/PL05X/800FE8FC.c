#include "manual.h"
#include "hex2c_rt.h"

/* PL05X 800FE8FC..800FEFE8 (1772 bytes). $a0 = P (object). S = *(*(P+0x80)+0x178).
 * Dispatch on byte S+0x1FA, index = byte - 1 into the jump table at 800F4A04 (15 entries).
 *   idx 0,6  -> 800FEA0C    idx 3 -> 800FEBA8    idx 4 -> 800FED28
 *   idx 7    -> 800FEDC4    idx 11 -> 800FEED4   idx 14 -> 800FE970
 *   idx 1,2,5,8,9,10,12,13 -> 800FEFC4 (the epilogue itself)
 * The lui $v0,0x800f in the delay slot of the range check runs on both paths, so an index >= 15 returns
 * 0x800F0000. A table entry that points at 800FEFC4 returns $v0 = 800FEFC4 (the loaded table word) with no store to
 * P+0x0C. Generated C had only the 0x800F0000 default, so the table-epilogue returns were wrong.
 *
 * Calls: rand (80025BF8) reads no argument register and writes only $a0, $v0, $v1. The allocator (80025548)
 * takes $a0 = (S+0x136)+4 and $a1 = 1, $a2 = 9 (set before every call here). It overwrites $a3 (addu a3,v0,a0 at
 * 80025564) before it reads any input, so the $a3 the caller passes is never read. The caller's $a3 is therefore
 * passed on unchanged, which is the entry $a3 for the first call and an unobservable callee leftover after that.
 * $a1 and $a2 are written before any read, so they are dummy parameters (nargs 4 keeps $a3 in place). */

static uint32_t man_PL05X_800FE8FC_rand(const GuestMem *m) {
    return m->call4(m->ctx, 0x80025BF8u, 0u, 0u, 0u, 0u);
}

/* jal 80025548 with a0 = (S+0x136)+4 (masked), a1 = 1, a2 = 9; if the result is non-null, fill the record:
 * +0x80 = S, +4 = flag, +0x0B = P+0x0B, +0x6A = P+0x72, +0x6E = P+0x76. */
static void man_PL05X_800FE8FC_spawn(const GuestMem *m, uint32_t p, uint32_t s0, uint32_t a3, uint32_t flag) {
    uint32_t v = m->call4(m->ctx, 0x80025548u, (RD8(s0 + 0x136u) + 4u) & 0xFFu, 1u, 9u, a3);
    if (v != 0u) {
        WR32(v + 0x80u, s0);
        WR8(v + 0x04u, flag);
        WR8(v + 0x0Bu, RD8(p + 0x0Bu));
        WR16(v + 0x6Au, RD16(p + 0x72u));
        WR16(v + 0x6Eu, RD16(p + 0x76u));
    }
}

uint32_t man_PL05X_800FE8FC(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;                                    /* s2 */
    uint32_t s0 = RD32(RD32(p + 0x80u) + 0x178u);      /* s0 = S */
    uint32_t h = RD16(p + 0x0Cu);                       /* lhu v1,0xc(s2) at 800fe930 */
    uint32_t r = h - 1u;                                /* addiu v0,v1,-1 in the bne delay slot (both paths) */
    uint32_t idx, res = 0u, s1, s3, s4, s5, t, v, b;

    (void)a1;
    (void)a2;

    if (h != 0u) {                                      /* bne v0,zero,800fefc0 */
        WR16(p + 0x0Cu, r);
        return r;
    }

    idx = (uint32_t)RD8(s0 + 0x1FAu) - 1u;             /* lbu v0,0x1fa(s0); addiu v1,v0,-1 */
    if (idx >= 15u)
        return 0x800F0000u;                             /* sltiu/beq to 800fefc4 with v0 = lui 0x800f0000 */

    switch (idx) {
    case 0:
    case 6:                                             /* byte 1 or 7: 800fea0c */
        s1 = 0u;                                        /* addu s1,zero,zero in the bne delay slot */
        if (RDS16(s0 + 0x6Eu) < 36) {                   /* lh v2+110 < 36: 800feaac */
            while (s1 < (man_PL05X_800FE8FC_rand(m) & 3u)) {
                t = man_PL05X_800FE8FC_rand(m);
                WR16(p + 0x72u, RD16(s0 + 0x6Au) + (t & 0x7Fu) - 64u);
                WR16(p + 0x76u, RD16(s0 + 0x6Eu));
                man_PL05X_800FE8FC_spawn(m, p, s0, a3, 0u);
                if (RD8(s0 + 0x0Bu) != 0u) {
                    t = man_PL05X_800FE8FC_rand(m);
                    v = RD16(s0 + 0x6Au) - ((t & 0x70u) - 48u);
                } else {
                    t = man_PL05X_800FE8FC_rand(m);
                    v = RD16(s0 + 0x6Au) + (t & 0x70u) - 48u;
                }
                WR16(p + 0x72u, v);
                s1 += 1u;
                WR16(p + 0x76u, RD16(s0 + 0x6Eu));
            }
            res = man_PL05X_800FE8FC_rand(m) & 7u;      /* 800feb98 */
        } else {                                        /* 800fea20 */
            while (s1 < (man_PL05X_800FE8FC_rand(m) & 7u)) {
                if (RD8(s0 + 0x0Bu) != 0u) {
                    t = man_PL05X_800FE8FC_rand(m);
                    v = RD16(s0 + 0x6Au) - ((t & 0x7Fu) - 48u);
                } else {
                    t = man_PL05X_800FE8FC_rand(m);
                    v = RD16(s0 + 0x6Au) + (t & 0x7Fu) - 48u;
                }
                WR16(p + 0x72u, v);
                s1 += 1u;
                WR16(p + 0x76u, RD16(s0 + 0x6Eu) + 48u);
            }
            res = (man_PL05X_800FE8FC_rand(m) & 7u) + 2u;   /* 800fea98 */
        }
        break;

    case 3:                                             /* byte 4: 800febA8 */
        s1 = 0u;                                        /* addu s1,zero,zero in the bne delay slot */
        if (RD8(s0 + 0x21Bu) == 2u) {                   /* lbu v1,0x21b(s0) == 2: 800febb8 */
            while (s1 < (man_PL05X_800FE8FC_rand(m) & 7u)) {
                if (RD8(s0 + 0x0Bu) != 0u) {
                    t = man_PL05X_800FE8FC_rand(m);
                    v = RD16(s0 + 0x6Au) - ((t & 0x70u) - 48u);
                } else {
                    t = man_PL05X_800FE8FC_rand(m);
                    v = RD16(s0 + 0x6Au) + (t & 0x70u) - 48u;
                }
                WR16(p + 0x72u, v);
                s1 += 1u;
                WR16(p + 0x76u, RD16(s0 + 0x6Eu) + 40u);
            }
        } else {                                        /* 800fec44 */
            s4 = 1u;
            s3 = 0x800FF670u;                           /* s3 = s5 */
            do {
                if (RD8(s0 + 0x0Bu) != 0u) {
                    v = RD16(s0 + 0x6Au) - (uint32_t)(int32_t)RDS8(s3);
                } else {
                    v = RD16(s0 + 0x6Au) + (uint32_t)(int32_t)RDS8(s3);
                }
                WR16(p + 0x72u, v);
                WR16(p + 0x76u, RD16(s0 + 0x6Eu) + (uint32_t)(int32_t)RDS8(0x800FF670u + s4));
                man_PL05X_800FE8FC_spawn(m, p, s0, a3, s1 & 1u);
                s4 += 2u;
                s1 += 1u;
                s3 += 2u;
            } while (s1 < 4u);
        }
        res = (man_PL05X_800FE8FC_rand(m) & 7u) + 4u;  /* 800fed14 / 800fec30 */
        break;

    case 4:                                             /* byte 5: 800fed28 */
        WR16(p + 0x6Eu, 35u);                           /* sh v0,0x6e(s2) */
        for (s1 = 0u; s1 < 4u; s1++) {
            t = man_PL05X_800FE8FC_rand(m);
            WR16(p + 0x72u, RD16(s0 + 0x6Au) + (t & 0x7Fu) - 64u);
            WR16(p + 0x76u, RD16(s0 + 0x6Eu));
            man_PL05X_800FE8FC_spawn(m, p, s0, a3, 0u);
        }
        res = 2u;                                       /* v0 = 2 in the loop bottom delay slot */
        break;

    case 7:                                             /* byte 8: 800fedc4 */
        b = RD8(s0 + 0x206u) - 2u;
        if (b >= 2u)
            return 0u;                                  /* sltiu result 0 returned via 800fefc4, no store */
        if (RDS16(s0 + 0x6Eu) < 36)
            return 1u;                                  /* slti result 1 returned via 800fefc4, no store */
        s1 = 2u;
        s4 = 5u;
        s5 = 0x800FF670u;
        s3 = s5 + 4u;
        do {
            if (RD8(s0 + 0x0Bu) != 0u) {
                v = RD16(s0 + 0x6Au) - (uint32_t)(int32_t)RDS8(s3);
            } else {
                v = RD16(s0 + 0x6Au) + (uint32_t)(int32_t)RDS8(s3);
            }
            WR16(p + 0x72u, v);
            WR16(p + 0x76u, RD16(s0 + 0x6Eu) + (uint32_t)(int32_t)RDS8(s5 + s4));
            man_PL05X_800FE8FC_spawn(m, p, s0, a3, s1 & 1u);
            s4 += 2u;
            s1 += 1u;
            s3 += 2u;
        } while (s1 < 4u);
        res = (man_PL05X_800FE8FC_rand(m) & 3u) + 2u;
        break;

    case 11:                                            /* byte 12: 800feed4 */
        s5 = 1u;
        s3 = 1u;
        s4 = 0x800FF678u;
        s1 = s4;                                        /* pointer into the table */
        do {
            if (RD8(s0 + 0x0Bu) != 0u) {
                v = RD16(s0 + 0x6Au) - (uint32_t)(int32_t)RDS8(s1);
            } else {
                v = RD16(s0 + 0x6Au) + (uint32_t)(int32_t)RDS8(s1);
            }
            WR16(p + 0x72u, v);
            b = RD16(s0 + 0x6Eu) + (uint32_t)(int32_t)RDS8(s4 + s3);
            WR16(p + 0x76u, b);
            if (RD8(s0 + 0x1FCu) == 1u)
                WR16(p + 0x76u, b - 24u);
            man_PL05X_800FE8FC_spawn(m, p, s0, a3, 1u);
            s1 += 2u;
            s3 += 2u;
        } while (s1 < s4 + 6u);
        res = 20u;
        break;

    case 14:                                            /* byte 15: 800fe970 */
        for (s1 = 0u; s1 < (man_PL05X_800FE8FC_rand(m) & 7u); s1++) {
            if (RD8(s0 + 0x0Bu) != 0u) {
                t = man_PL05X_800FE8FC_rand(m);
                v = RD16(s0 + 0x6Au) - ((t & 0x7Fu) - 48u);
            } else {
                t = man_PL05X_800FE8FC_rand(m);
                v = RD16(s0 + 0x6Au) + (t & 0x7Fu) - 48u;
            }
            WR16(p + 0x72u, v);
            t = man_PL05X_800FE8FC_rand(m);
            WR16(p + 0x76u, RD16(s0 + 0x6Eu) + (t & 0x0Fu) - 16u);
        }
        res = (man_PL05X_800FE8FC_rand(m) & 7u) + 2u;  /* 800fe9f8 */
        break;

    case 1:
    case 2:
    case 5:
    case 8:
    case 9:
    case 10:
    case 12:
    case 13:
        return 0x800FEFC4u;                             /* jr to the epilogue with $v0 = the table word */
    default:
        return 0x800F0000u;
    }

    WR16(p + 0x0Cu, res);                               /* 800fefc0: sh v0,0xc(s2) */
    return res;
}
