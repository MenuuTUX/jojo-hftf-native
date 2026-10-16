#include "manual.h"
#include "hex2c_rt.h"

/* VS_SEL 800DFC48..800DFCB0. $a0 = P (kept in s0). Sets byte P+2 = byte P+2 + 1 in the delay slot of jal 80025548 with
 * $a0 = 8, $a1 = 12, $a2 = 1 (and $a3 as received). The returned pointer R gets byte R+4 = 2 and word R+0x80 = P (delay slot
 * of jal 800dfdf0). The 800dfdf0 call takes $a0..$a3 as 80025548 left them (gpr_get after call4). If its $v0 is non-zero,
 * jal 80025548 again with $a0 = 8, $a1 = 12, $a2 = 0 (and the $a3 that 800dfdf0 left), and byte R2+4 = 2. Returns $v0. */

static void reload_args(const GuestMem *m, uint32_t *a0, uint32_t *a1, uint32_t *a2, uint32_t *a3) {
    *a0 = m->gpr_get(m->ctx, 4u);
    *a1 = m->gpr_get(m->ctx, 5u);
    *a2 = m->gpr_get(m->ctx, 6u);
    *a3 = m->gpr_get(m->ctx, 7u);
}

uint32_t man_VS_SEL_800DFC48(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;                                      /* addu s0,a0,zero */
    uint32_t sp = m->sp_get(m->ctx) - 32u;               /* addiu sp,sp,-32 */
    uint32_t v0, r, rr;
    m->sp_set(m->ctx, sp);

    a0 = 8u;                                              /* addiu a0,zero,8 */
    a1 = 12u;                                             /* addiu a1,zero,12 */
    a2 = 1u;                                              /* addiu a2,zero,1 */
    v0 = RD8(p + 2u) + a2;                                /* lbu v0,2(s0); addu v0,v0,a2 */
    WR8(p + 2u, v0);                                      /* sb v0,2(s0): delay slot of jal 80025548 */
    r = m->call4(m->ctx, 0x80025548u, a0, a1, a2, a3);    /* jal 80025548 */
    reload_args(m, &a0, &a1, &a2, &a3);
    WR8(r + 4u, 2u);                                      /* addiu s1,zero,2; sb s1,4(v0) */
    WR32(r + 0x80u, p);                                   /* jal 800dfdf0; delay slot sw s0,0x80(v0) */
    rr = m->call4(m->ctx, 0x800DFDF0u, a0, a1, a2, a3);
    reload_args(m, &a0, &a1, &a2, &a3);
    a0 = 8u;                                              /* beq delay slot addiu a0,zero,8 (both paths) */
    if (rr == 0u)                                         /* beq v0,zero,800dfca0: return v0 = 0 */
        goto L_end;

    a1 = 12u;                                             /* addiu a1,zero,12 */
    a2 = 0u;                                              /* jal delay slot addu a2,zero,zero */
    r = m->call4(m->ctx, 0x80025548u, a0, a1, a2, a3);    /* jal 80025548 */
    reload_args(m, &a0, &a1, &a2, &a3);
    WR8(r + 4u, 2u);                                      /* sb s1,4(v0) after the return */
    rr = r;

L_end:
    m->sp_set(m->ctx, sp + 32u);                          /* epilogue */
    return rr;
}
