#include "manual.h"
#include "hex2c_rt.h"

/* PL19 800E3380..800E3464 (slot A). Same first half as 800E30CC (first call, 0x6A/0x6E copy, 800BB784 call), but the a0 != 0 and != 1 path calls 80026a04 at once
 * (no $4E == 129 test; delay slot addu a0,s1). */
/* $a0 = P (struct). $a1..$a3 are passed on unchanged to the first callee,
 * 800E3BCC (it writes only $v0, $v1). 800BB784 writes $a1 (its first instructions set it to 0x80066CA0), so the calls after it take $a1 from
 * exit_reg(5) read right after that call. 80026A04 and 80026AFC never read $a1 before writing it.
 * $a2 and $a3 are never written here or by the callees above: they pass through unchanged.
 * Branch delay slots run on both paths: the "addiu v0,zero,3" after the first bne, the "addiu v0,zero,8"
 * path, "addiu v0,zero,1" and "addiu v0,zero,129" before the byte compares, and "addiu v0,a0,1" after
 * the 0x4E compare all set $v0 on the taken and the fall-through path. */
uint32_t man_PL19_800E3380(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;                                    /* s1 */
    uint32_t s0 = RD32(p + 0x80u);                      /* lw s0,0x80(s1) */
    uint32_t r, a0x, a1x;

    r = m->call4(m->ctx, 0x800E3BCCu, p, a1, a2, a3);   /* jal 800e3bcc, a0 = P */
    if (r != 0) {                                       /* bne v0,zero,800e31b0; delay slot v0 = 3 */
        WR8(p + 0x05u, 3u);                             /* sb v0,0x5(s1) */
        return 3u;
    }
    if (RD8(s0 + 0x1FAu) != 8u) {                       /* lbu v1,0x1fa(s0); bne v1,v0(=8) */
        WR8(p + 0x05u, 3u);
        return 3u;
    }
    WR16(p + 0x6Au, RD16(s0 + 0x6Au));                  /* lhu v0,0x6a(s0); sh v0,0x6a(s1) */
    WR16(p + 0x6Eu, RD16(s0 + 0x6Eu));                  /* lhu v0,0x6e(s0); jal delay slot sh v0,0x6e(s1) */
    r = m->call4(m->ctx, 0x800BB784u, s0, a1, a2, a3);  /* addu a0,s0,zero before the jal */
    a1x = m->exit_reg(m->ctx, 5);                       /* $a1 as 800BB784 left it, before the caller registers return */
    if (r != 0)                                         /* bne v0,zero,800e31b4 */
        return r;

    a0x = RD8(p + 0x06u);                               /* lbu a0,6(s1) */
    if (a0x == 0u) {                                    /* andi v1,a0,0xff; bne v1,zero (not taken) */
        if (RD8(s0 + 0x4Eu) != 0x80u)                   /* lbu v1,0x4e(s0); bne v1,v0(=0x80) */
            return a0x + 1u;                            /* delay slot addiu v0,a0,1 */
        WR8(p + 0x06u, a0x + 1u);                       /* sb v0,6(s1) */
        WR8(p + 0x01u, 1u);                             /* j 800e31b4; delay slot sb v0,1(s1), v0 = 1 */
        return 1u;
    }
    if (a0x == 1u) {                                    /* bne v1,v0(=1) not taken */
        if (RD8(s0 + 0x4Eu) != 129u)                    /* bne v1,v0(=129) */
            return a0x + 1u;                            /* delay slot addiu v0,a0,1 */
        WR8(p + 0x06u, a0x + 1u);                       /* sb v0,6(s1) */
        return m->call4(m->ctx, 0x80026AFCu, p, a1x, a2, a3); /* jal 80026afc, a0 = s1 */
    }
    /* a0 != 0 and != 1: bne v1,v0(=1) taken to 800e3188 with v0 = 129 (delay slot) */
    m->call4(m->ctx, 0x80026A04u, p, a1x, a2, a3); /* jal 80026a04, a0 = s1 */
    if (RDS8(p + 0x4Du) == -1)                          /* lb v1,0x4d(s1); bne v1,v0(=-1) */
        WR8(p + 0x05u, 3u);                             /* fall-through sb v0,5(s1); v0 = 3 (delay slot) */
    return 3u;
}
