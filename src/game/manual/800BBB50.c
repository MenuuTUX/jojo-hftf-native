#include "manual.h"
#include "hex2c_rt.h"

/* 800BBB50 (AC_MAIN). $a0 = P (struct). $a1 is overwritten (lbu a1,0xb(s0)) before any read, so it is only
 * a dummy parameter here (nargs 4 keeps $a2 and $a3 as parameters 3 and 4). $a2 and $a3 are never written and
 * are passed on unchanged to the three callees. $a1 at each call is B = byte at (struct S+0x0B), as the original
 * leaves it. */
uint32_t man_800BBB50(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;                                    /* s2 */
    uint32_t s0 = RD32(p + 0x15Cu);                     /* lw s0,0x15c(s2) at 800bbb70 */
    uint32_t s3 = RD32(s0 + 0x170u);                    /* lw s3,0x170(s0) at 800bbb7c */
    uint32_t b = RD8(s0 + 0x0Bu);                       /* lbu a1,0xb(s0) at 800bbb84 */
    uint32_t tbl = RD32(0x800CB940u + 4u * (uint32_t)(int32_t)RDS8(s0 + 0x136u));
    uint32_t s1 = tbl + 8u * RD8(s0 + 0x141u);          /* s1 = entry pointer, 800bba0 */
    uint32_t t1, t2, v0, v1, d;

    WR8(p + 0x0Bu, b ^ 1u);                             /* xori v0,a1,1; sb v0,0xb(s2) */
    if ((RD8(s1 + 5u) & 0x80u) != 0)
        WR8(p + 0x0Bu, b);                              /* sb a1,0xb(s2) at 800bbbb8 */
    WR8(p + 0x165u, (RD8(s1 + 5u) & 0x40u) != 0 ? 1u : 0u);

    if (RD8(s0 + 0x141u) == RD8(s0 + 0x142u)) {
        m->call4(m->ctx, 0x80026A04u, p, b, a2, a3);    /* jal 80026a04, a0 = s2 in delay slot */
    } else {
        uint32_t idx = (uint32_t)(int32_t)RDS8(s0 + 0x136u);
        uint32_t a1x = RD32(RD32(0x800CB938u + 4u * idx) + 4u * RD8(s1 + 4u));
        m->call4(m->ctx, 0x800268BCu, p, a1x, a2, a3);  /* jal 800268bc at 800bbc24 */
    }

    if ((RD8(s1 + 5u) & 0x10u) != 0)
        WR8(p + 0x12u, RD8(p + 0x12u) | 2u);            /* ori v0,v0,2 */
    else
        WR8(p + 0x12u, RD8(p + 0x12u) & 0xFDu);         /* andi v0,v0,0xfd */

    v0 = RD8(s1 + 5u) & 0x20u;                          /* andi v0,v0,0x20 at 800bbc60 */
    if (v0 != 0)
        return v0;

    t1 = RD16(s1 + 0u);                                 /* lhu v1,0(s1) at 800bbc74 */
    if (RD8(s0 + 0x0Bu) != 0)
        t1 = 0u - t1;                                   /* subu v1,zero,v1 */
    WR16(p + 0x6Au, RD16(s0 + 0x6Au) + t1);
    t2 = RD16(s0 + 0x6Eu) + RD16(s1 + 2u);
    v1 = RD16(p + 0x6Au);                               /* lhu v1,0x6a(s2) at 800bbca4, after the store above */
    WR16(p + 0x6Eu, t2);
    s1 = v1 - RD16(s0 + 0x6Au);                         /* subu s1,v1,v0: delay slot of jal 800bb690 */
    m->call4(m->ctx, 0x800BB690u, p, b, a2, a3);

    v0 = RD8(p + 0x254u);                               /* lbu v0,0x254(s2) at 800bbcbc */
    if (v0 == 0)
        return 0u;

    /* delay slot of beq at 800bbcc4 sets $a0 = 2 */
    WR16(s0 + 0x6Au, RD16(p + 0x6Au) - s1);             /* sh v0,0x6a(s0) */
    v0 = RD8(s0 + 0x1Au);
    if (v0 == 2u)
        return 8u;   /* the delay slot of beq at 800bbce4 (addiu v0,zero,8) runs on the taken branch too (replay phase2-1172) */
    if (RD8(s0 + 0x137u) == 8u)
        return 8u;
    v0 = RD8(s0 + 0x1FBu);
    if (v0 == 2u)
        return v0;

    d = RD16(p + 0x6Au) - s1;                           /* lhu v0,0x6a(s2); subu v0,v0,s1 */
    WR16(s3 + 0x6Au, d);                                /* sh v0,0x6a(s3) */
    return d;
}
