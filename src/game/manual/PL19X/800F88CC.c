#include "manual.h"
#include "hex2c_rt.h"

#define S8_(x) ((uint32_t)(int32_t)(int8_t)(x))

/* 800F88CC (PL19X, slot B, 252 bytes). $a0 = P (kept in s1). $a1..$a3 are passed through unchanged to the first callee,
 * 800f93cc, so nargs is 4. Every callee may change $a0..$a3; they are reloaded with gpr_get after each call.
 * Branch delay slots run on both paths. A branch tests the register value from before its delay slot: the bne at 8944
 * tests v1 against 128 before its slot (addu v0,a0,1) runs, and the bne at 896c tests v1 against 129 before its slot.
 * $v0 at jr ra: 3 on the paths that store P+5 (89b0), 1 after the sb at 8958, 129 at 89b4 when reached from 8990, or the
 * callee's $v0 from the jal 80026afc / 800bb784 / 80026a04 exits. */
uint32_t man_PL19X_800F88CC(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
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
    t = (v0 != 0u);                                 /* bne v0,zero,89b0 tests before its delay slot */
    v0 = 3u;                                        /* addiu v0,zero,3: delay slot, runs on both paths */
    if (t) goto L89B0;

    v1 = RD8(s0 + 0x1FAu);                          /* lbu v1,0x1fa(s0) */
    v0 = 8u;                                        /* addiu v0,zero,8 */
    t = (v1 != v0);                                 /* bne v1,v0,89b0 */
    v0 = 3u;                                        /* addiu v0,zero,3: delay slot */
    if (t) goto L89B0;

    WR16(p + 0x6Au, RD16(s0 + 0x6Au));              /* lhu v0,0x6a(s0); sh v0,0x6a(s1) */
    v0 = RD16(s0 + 0x6Eu);                          /* lhu v0,0x6e(s0) */
    a0 = s0;                                        /* addu a0,s0,zero */
    WR16(p + 0x6Eu, v0);                            /* sh v0,0x6e(s1): delay slot of jal 800bb784 */
    v0 = m->call4(m->ctx, 0x800BB784u, a0, a1, a2, a3);
    REFRESH();
    if (v0 != 0u) goto L89B4;                       /* bne v0,zero,89b4 */

    a0 = RD8(p + 0x06u);                            /* lbu a0,6(s1) */
    v1 = a0 & 0xFFu;                                /* andi v1,a0,0xff */
    v0 = 1u;                                        /* addiu v0,zero,1: bne delay slot, runs on both paths */
    if (v1 != 0u) goto L895C;                       /* bne v1,zero,895c */

    /* 893c: v1 == 0 */
    v1 = RD8(s0 + 0x4Eu);                           /* lbu v1,0x4e(s0) */
    v0 = 128u;                                      /* addiu v0,zero,128 */
    t = (v1 != v0);                                 /* bne v1,v0,89b4 */
    v0 = a0 + 1u;                                   /* addiu v0,a0,1: delay slot, runs on both paths */
    if (t) goto L89B4;
    WR8(p + 0x06u, v0);                             /* sb v0,6(s1) */
    v0 = 1u;                                        /* addiu v0,zero,1 */
    WR8(p + 0x01u, v0);                             /* j 89b4; sb v0,1(s1): delay slot */
    goto L89B4;

L895C:
    /* v0 = 1 here */
    t = (v1 != v0);                                 /* bne v1,v0,8988 */
    v0 = 129u;                                      /* addiu v0,zero,129: delay slot, runs on both paths */
    if (t) goto L8988;
    v1 = RD8(s0 + 0x4Eu);                           /* lbu v1,0x4e(s0) (8964) */
    t = (v1 != v0);                                 /* bne v1,v0,89b4 */
    v0 = a0 + 1u;                                   /* addiu v0,a0,1: delay slot */
    if (t) goto L89B4;
    WR8(p + 0x06u, v0);                             /* sb v0,6(s1) */
    a0 = p;                                         /* addu a0,s1,zero: delay slot of jal 80026afc */
    v0 = m->call4(m->ctx, 0x80026AFCu, a0, a1, a2, a3);
    REFRESH();
    goto L89B4;

L8988:
    v1 = RD8(s0 + 0x4Eu);                           /* lbu v1,0x4e(s0) */
    t = (v1 == v0);                                 /* beq v1,v0,89b4 (v0 = 129) */
    if (t) goto L89B4;                              /* delay slot nop */
    a0 = p;                                         /* addu a0,s1,zero: delay slot of jal 80026a04 */
    v0 = m->call4(m->ctx, 0x80026A04u, a0, a1, a2, a3);
    REFRESH();
    v1 = S8_(RD8(p + 0x4Du));                       /* lb v1,0x4d(s1) */
    v0 = 0xFFFFFFFFu;                               /* addiu v0,zero,-1 */
    t = (v1 != v0);                                 /* bne v1,v0,89b4 */
    v0 = 3u;                                        /* addiu v0,zero,3: delay slot, runs on both paths */
    if (t) goto L89B4;
    goto L89B0;

L89B0:
    WR8(p + 0x05u, v0);                             /* sb v0,5(s1): v0 = 3 on every path that gets here */
    return v0;

L89B4:
    return v0;

#undef REFRESH
}
