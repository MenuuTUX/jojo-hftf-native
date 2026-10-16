#include "manual.h"
#include "hex2c_rt.h"

#define S8_(x) ((uint32_t)(int32_t)(int8_t)(x))

/* 800F9290 (PL19X, slot B, 212 bytes). $a0 = P (kept in s1). $a1..$a3 pass through unchanged to the first callee,
 * 800f93cc, so nargs is 4. Callees may change $a0..$a3; they are reloaded with gpr_get after each call. Branch delay
 * slots run on both paths; a branch tests the value from before its slot (the bne at 9308 tests v1 against 1 before the
 * addiu v0,zero,129 at 930c; the bne at 92dc tests v1 = P+6 & 0xff before the sh of 0x6e runs).
 * This is not the same routine as 800F88CC: there is no call to 800bb784 and the v1 == 1 branch goes straight to the
 * jal 80026a04 when v1 != 129. $v0 at jr ra: 3 on the path that stores P+5, 1 after the sb of P+1 (path 92f0-9300), the
 * callee's $v0 from the jal 80026a04 / 80026afc exits, otherwise the value left by the branch. */
uint32_t man_PL19X_800F9290(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;                                /* s1 */
    uint32_t s0, v0, v1, t;

#define REFRESH() do {                                       \
        a0 = m->gpr_get(m->ctx, 4u);                         \
        a1 = m->gpr_get(m->ctx, 5u);                         \
        a2 = m->gpr_get(m->ctx, 6u);                         \
        a3 = m->gpr_get(m->ctx, 7u);                         \
    } while (0)

    s0 = RD32(p + 0x80u);                           /* lw s0,0x80(s1) */
    v0 = m->call4(m->ctx, 0x800F93CCu, p, a1, a2, a3);   /* jal 800f93cc, delay slot nop */
    REFRESH();
    t = (v0 != 0u);                                 /* bne v0,zero,934c tests before its delay slot */
    v0 = 3u;                                        /* addiu v0,zero,3: delay slot */
    if (t) goto L934C;

    v1 = RD8(s0 + 0x1FAu);                          /* lbu v1,0x1fa(s0) */
    v0 = 8u;                                        /* addiu v0,zero,8 */
    t = (v1 != v0);                                 /* bne v1,v0,934c */
    v0 = 3u;                                        /* addiu v0,zero,3: delay slot */
    if (t) goto L934C;

    WR16(p + 0x6Au, RD16(s0 + 0x6Au));              /* lhu v0,0x6a(s0); sh v0,0x6a(s1) */
    a0 = RD8(p + 0x06u);                            /* lbu a0,6(s1) */
    v1 = a0 & 0xFFu;                                /* andi v1,a0,0xff */
    WR16(p + 0x6Eu, RD16(s0 + 0x6Eu));             /* lhu v0,0x6e(s0): sh v0,0x6e(s1) is the bne delay slot */
    if (v1 != 0u) goto L9304;                       /* bne v1,zero,9304 */

    /* 92e4: v1 == 0 */
    v1 = RD8(s0 + 0x4Eu);                           /* lbu v1,0x4e(s0) */
    v0 = 128u;                                      /* addiu v0,zero,128 */
    t = (v1 != v0);                                 /* bne v1,v0,9350 */
    v0 = a0 + 1u;                                   /* addiu v0,a0,1: delay slot, runs on both paths */
    if (t) goto L9350;
    WR8(p + 0x06u, v0);                             /* sb v0,6(s1) */
    v0 = 1u;                                        /* addiu v0,zero,1 */
    WR8(p + 0x01u, v0);                             /* j 9350; sb v0,1(s1): delay slot */
    goto L9350;

L9304:
    v0 = 1u;                                        /* addiu v0,zero,1 */
    t = (v1 != v0);                                 /* bne v1,v0,9334 */
    v0 = 129u;                                      /* addiu v0,zero,129: delay slot */
    if (t) goto L9334;
    v1 = RD8(s0 + 0x4Eu);                           /* lbu v1,0x4e(s0) */
    t = (v1 != v0);                                 /* bne v1,v0,9350 (v0 = 129) */
    v0 = a0 + 1u;                                   /* addiu v0,a0,1: delay slot */
    if (t) goto L9350;
    WR8(p + 0x06u, v0);                             /* sb v0,6(s1) */
    a0 = p;                                         /* addu a0,s1,zero: delay slot of jal 80026afc */
    v0 = m->call4(m->ctx, 0x80026AFCu, a0, a1, a2, a3);
    REFRESH();
    goto L9350;

L9334:
    a0 = p;                                         /* addu a0,s1,zero: delay slot of jal 80026a04 */
    v0 = m->call4(m->ctx, 0x80026A04u, a0, a1, a2, a3);
    REFRESH();
    v1 = S8_(RD8(p + 0x4Du));                       /* lb v1,0x4d(s1) */
    v0 = 0xFFFFFFFFu;                               /* addiu v0,zero,-1 */
    t = (v1 != v0);                                 /* bne v1,v0,9350 */
    v0 = 3u;                                        /* addiu v0,zero,3: delay slot, runs on both paths */
    if (t) goto L9350;

L934C:
    WR8(p + 0x05u, v0);                             /* sb v0,5(s1): v0 = 3 on every path that gets here */
    return v0;

L9350:
    return v0;

#undef REFRESH
}
