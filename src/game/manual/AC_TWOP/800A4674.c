#include "manual.h"
#include "hex2c_rt.h"

/* AC_TWOP 800A4674..800A467C. Two instructions, then the body of 800A467C (64 bytes) follows with no jump:
 *   lui v0,0x8006 / lbu v0,0x6cb2(v0)   -> v0 = RD8(0x80066CB2), the input to the fall-through body.
 * Hex-Rays showed this as `return sub_800A467C()`, which drops the lbu and the zero-test on v0.
 * The body (frame 24, sw ra at +16): if v0 != 0 it returns v0 (the byte). Else count = RD16(a0+0xC) - 1, stored back,
 * a0+0x2C cleared; if (count << 16) > 0 (signed) it returns that, otherwise it calls 800A4BB0(a0, a1, a2, a3) and returns
 * its $v0. $a2 and $a3 are passed through to the call unchanged, so they are parameters 3 and 4. */
uint32_t man_AC_TWOP_800A4674(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t entry_sp, frame_sp, entry_ra, v0;

    v0 = RD8(0x80066CB2u);                              /* lui v0,0x8006; lbu v0,0x6cb2(v0) */
    entry_sp = m->sp_get(m->ctx);
    entry_ra = m->gpr_get(m->ctx, 31u);
    frame_sp = entry_sp - 24u;                          /* addiu sp,sp,-24 at 800a467c */
    m->sp_set(m->ctx, frame_sp);
    WR32(frame_sp + 16u, entry_ra);                     /* sw ra,0x10(sp), delay slot of bne, runs on both paths */

    if (v0 == 0u) {                                     /* bne v0,zero,800a46ac not taken */
        v0 = RD16(a0 + 0x0Cu);                          /* lhu v0,0xc(a0) */
        WR8(a0 + 0x2Cu, 0u);                            /* sb zero,0x2c(a0) */
        v0 = v0 - 1u;                                   /* addiu v0,v0,-1 */
        WR16(a0 + 0x0Cu, v0);                           /* sh v0,0xc(a0) */
        v0 = v0 << 16;                                  /* sll v0,v0,16 */
        if ((int32_t)v0 <= 0) {                         /* bgtz v0 not taken: delay slot nop, then jal 800a4bb0 */
            v0 = m->call4(m->ctx, 0x800A4BB0u, a0, a1, a2, a3);
        }
    }

    m->sp_set(m->ctx, frame_sp + 24u);                  /* lw ra,0x10(sp); jr ra; addiu sp,sp,24 */
    return v0;
}
