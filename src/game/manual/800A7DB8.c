#include "manual.h"
#include "hex2c_rt.h"

/* 800A7DB8: reads no argument register (a0..a3 are all set before use). Counts down the byte at 0x80066CC1 and returns
 * (count << 24) while it is nonzero. Otherwise it picks two of the three records (0x800CDD50, 0x800CD268, their +0x390
 * partners 0x800CE0E0 and 0x800CD5F8) by the byte at +0x230 and the flag at 0x80066C61 bit 0, runs 80024104(s0, 142)
 * and resets both records, then returns 8 (the value $v0 holds from 800A7EF8 on). */
uint32_t man_800A7DB8(const GuestMem *m) {
    const uint32_t s4 = 0x80066CA0u;        /* lui v0,0x8006; addiu s4,v0,0x6CA0 */
    const uint32_t t0 = 0x80066C48u;        /* lui v0,0x8006; addiu t0,v0,0x6C48 */
    const uint32_t a0 = 0x800CDD50u;        /* lui v0,0x800d; addiu a0,v0,-8880 */
    const uint32_t a1 = 0x800CD268u;        /* addiu a1,v0,-11672 */
    const uint32_t a2 = 0x800CD268u + 0x390u;   /* addiu a2,a1,912 */
    const uint32_t a3 = 0x800CDD50u + 0x390u;   /* addiu a3,a0,912 */
    uint32_t s0, s1, s2, s3;
    uint32_t bB, bC, cnt, v0, v1;

    v0 = (uint32_t)(int32_t)RDS8(s4 + 0x20u);
    if (v0 == 0u) {
        return v0;
    }
    cnt = (RD8(s4 + 0x21u) - 1u) & 0xFFu;
    WR8(s4 + 0x21u, cnt);
    v0 = cnt << 24;
    if (v0 != 0u) {
        return v0;
    }

    bB = RD8(a1 + 0x230u);
    bC = RD8(a2 + 0x230u);
    if (bB == bC) {
        if ((RD8(t0 + 0x19u) & 1u) == 0u) {
            s0 = a0; s1 = a1; s2 = a3; s3 = a2;   /* 800A7E6C path */
        } else {
            s0 = a3; s1 = a2; s2 = a0; s3 = a1;   /* 800A7E58 path */
        }
    } else if (bC < bB) {
        s0 = a3; s1 = a2; s2 = a0; s3 = a1;       /* sltu at 800A7E34 set, bne taken to 800A7E5C */
    } else {
        s0 = a0; s1 = a1; s2 = a3; s3 = a2;       /* 800A7E40 j to 800A7E70 */
    }

    m->call4(m->ctx, 0x80024104u, s0, 142u, a2, a3);

    WR8(s4 + 0x20u, 0u);
    WR8(s0 + 0x107u, 30u);
    WR8(s2 + 0x107u, 30u);
    WR8(s3 + 0x107u, 30u);
    WR8(s1 + 0x1u, 0u);
    WR8(s1 + 0x0u, 0u);
    WR8(s0 + 0x7u, 25u);
    WR8(s0 + 0x149u, 8u);
    WR8(s0 + 0x1u, 1u);
    WR8(s0 + 0x6u, 1u);
    WR8(s0 + 0x8u, 0u);
    WR8(s0 + 0x14Au, 4u);
    WR8(s1 + 0x1FBu, 0u);
    WR8(s0 + 0x1FBu, 0u);
    WR8(s1 + 0x205u, 0u);
    WR8(s0 + 0x205u, 0u);
    v1 = RD8(s2 + 0xBu);
    WR8(s0 + 0x14Bu, 0u);
    WR8(s0 + 0x107u, 60u);
    WR8(s0 + 0x204u, 1u);
    WR8(s0 + 0x106u, v1);
    if ((int32_t)RDS8(s1 + 0x137u) != 8) {
        WR8(s1 + 0x204u, 0u);
    }
    WR32(s0 + 0x108u, s2);
    WR8(s2 + 0x7u, 1u);
    WR8(s3 + 0x7u, 1u);
    WR8(s3 + 0x8u, 0u);
    WR8(s2 + 0x8u, 0u);
    WR8(s3 + 0xAu, 0u);
    WR8(s2 + 0x9u, 0u);
    WR8(s3 + 0xAu, 0u);
    WR8(s2 + 0x9u, 0u);
    return 8u;
}
