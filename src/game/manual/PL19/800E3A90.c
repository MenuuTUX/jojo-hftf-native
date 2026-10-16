#include "manual.h"
#include "hex2c_rt.h"

/* PL19 800E3A90..800E3B60 (slot A). $a0 = P. Same first call and same 0x6A/0x6E copy as 800E30CC, but this
 * variant never calls 800BB784: after the 0x1FA == 8 test it reads P+6 and copies 0x6E, then branches on P+6.
 * So $a1 is still the entry value at the calls to 80026AFC and 80026A04 (800E3BCC does not write it). The
 * a0 != 0 and != 1 path calls 80026A04 at once (no $4E == 129 test). $a2 and $a3 pass through unchanged.
 * Branch delay slots run on both paths: the addiu v0 = 3 after the first bne, the addiu v0 = 8, the
 * addiu v0,a0,1 after the 0x4E compares, and the addiu v0 = 1 / 129 before the byte compares. */
uint32_t man_PL19_800E3A90(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;                                    /* s1 */
    uint32_t s0 = RD32(p + 0x80u);                      /* lw s0,0x80(s1) */
    uint32_t r, a0x, t6a;

    r = m->call4(m->ctx, 0x800E3BCCu, p, a1, a2, a3);   /* jal 800e3bcc, a0 = P */
    if (r != 0) {                                       /* bne v0,zero,800e3b4c; delay slot v0 = 3 */
        WR8(p + 0x05u, 3u);                             /* sb v0,0x5(s1) */
        return 3u;
    }
    if (RD8(s0 + 0x1FAu) != 8u) {                       /* lbu v1,0x1fa(s0); bne v1,v0(=8) */
        WR8(p + 0x05u, 3u);
        return 3u;
    }
    t6a = RD16(s0 + 0x6Au);                             /* lhu v0,0x6a(s0) */
    a0x = RD8(p + 0x06u);                               /* lbu a0,6(s1) */
    WR16(p + 0x6Au, t6a);                               /* sh v0,0x6a(s1) */
    WR16(p + 0x6Eu, RD16(s0 + 0x6Eu));                  /* lhu v0,0x6e(s0); sh v0,0x6e(s1) in the delay slot */
    if (a0x == 0u) {                                    /* andi v1,a0,0xff; bne v1,zero (not taken) */
        if (RD8(s0 + 0x4Eu) != 0x80u)                   /* lbu v1,0x4e(s0); bne v1,v0(=0x80) */
            return a0x + 1u;                            /* delay slot addiu v0,a0,1 */
        WR8(p + 0x06u, a0x + 1u);                       /* sb v0,6(s1) */
        WR8(p + 0x01u, 1u);                             /* j 800e3b50; delay slot sb v0,1(s1), v0 = 1 */
        return 1u;
    }
    if (a0x == 1u) {                                    /* bne v1,v0(=1) not taken */
        if (RD8(s0 + 0x4Eu) != 129u)                    /* bne v1,v0(=129) */
            return a0x + 1u;                            /* delay slot addiu v0,a0,1 */
        WR8(p + 0x06u, a0x + 1u);                       /* sb v0,6(s1) */
        return m->call4(m->ctx, 0x80026AFCu, p, a1, a2, a3); /* jal 80026afc, a0 = s1 */
    }
    m->call4(m->ctx, 0x80026A04u, p, a1, a2, a3);       /* jal 80026a04, a0 = s1 */
    if (RDS8(p + 0x4Du) == -1)                          /* lb v1,0x4d(s1); bne v1,v0(=-1) */
        WR8(p + 0x05u, 3u);                             /* fall-through sb v0,5(s1); v0 = 3 (delay slot) */
    return 3u;
}
