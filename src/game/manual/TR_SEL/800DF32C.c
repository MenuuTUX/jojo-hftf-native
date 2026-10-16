#include "manual.h"
#include "hex2c_rt.h"

/* TR_SEL 800DF32C..800DF3FB (208 bytes). $a0 and $a1 are overwritten before any read (dummy parameters); $a2 and $a3 are
 * passed on unchanged to the first callee. S = 0x80066C48 (lui/addiu at 800df330). Bit 0 of byte S+0x22 selects the record
 * (bit set: base 0x8006CEB8, record 0x800E2A58; bit clear: base 0x8006CEC8, record 0x800E2A78). The record gets halfword
 * base+6 at +0x10 and halfword base+2 at +0xE. Then: bit 0x100 of the record's +0x10 stores 5 to S+4 and 0 to S+6 (the
 * delay slot of jal 800df624) and calls 800df624; bit 0x800 calls 800dfad4; otherwise the chain 800df6a8, 800e0054,
 * 80029300 (a0 = 7) and 8001f0a8 runs, each call taking the registers the previous callee left (exit_reg). $ra is saved
 * at sp+0x10 and restored, as the original prologue and epilogue do. Returns the last callee's $v0. */
uint32_t man_TR_SEL_800DF32C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const uint32_t s = 0x80066C48u;
    uint32_t sp, base, v5, v4, r, l0, l1, l2, l3;
    (void)a0;
    (void)a1;

    sp = m->sp_get(m->ctx) - 24u;                        /* addiu sp,sp,-24 */
    m->sp_set(m->ctx, sp);
    WR32(sp + 16u, m->gpr_get(m->ctx, 31u));            /* sw ra,0x10(sp) */

    if (RD8(s + 0x22u) & 1u) {                           /* andi v0,v0,1; beq at 800df348 not taken */
        base = 0x8006CEB8u;
        v5 = 0x800E2A58u;
    } else {
        base = 0x8006CEC8u;
        v5 = 0x800E2A78u;
    }
    v4 = RD16(base + 6u);                                /* lhu v1,0x6(v0) */
    WR16(v5 + 0x10u, v4);                                /* sh v1,0x10(a0) */
    WR16(v5 + 0xEu, RD16(base + 2u));                    /* lhu v0,0x2(v0); sh v0,0xe(a0) */

    if (RD16(v5 + 0x10u) & 0x100u) {                     /* lhu v1,0x10(a0); andi v0,v1,0x100; beq at 800df394 */
        WR16(s + 4u, 5u);                                /* sh v0,0x4(a1), v0 = 5 (addiu in the beq delay slot) */
        WR16(s + 6u, 0u);                                /* sh zero,0x6(a1): delay slot of jal 800df624 */
        r = m->call4(m->ctx, 0x800DF624u, v5, s, a2, a3);
    } else if (RD16(v5 + 0x10u) & 0x800u) {              /* andi v0,v1,0x800; beq at 800df3b4 not taken */
        r = m->call4(m->ctx, 0x800DFAD4u, v5, s, a2, a3); /* jal 800dfad4, delay nop */
    } else {
        m->call4(m->ctx, 0x800DF6A8u, v5, s, a2, a3);    /* jal 800df6a8, delay nop */
        l0 = m->exit_reg(m->ctx, 4u);                    /* $a0..$a3 as 800df6a8 left them */
        l1 = m->exit_reg(m->ctx, 5u);
        l2 = m->exit_reg(m->ctx, 6u);
        l3 = m->exit_reg(m->ctx, 7u);
        m->call4(m->ctx, 0x800E0054u, l0, l1, l2, l3);   /* jal 800e0054, delay nop */
        l1 = m->exit_reg(m->ctx, 5u);                    /* $a1..$a3 as 800e0054 left them; $a0 is set to 7 below */
        l2 = m->exit_reg(m->ctx, 6u);
        l3 = m->exit_reg(m->ctx, 7u);
        m->call4(m->ctx, 0x80029300u, 7u, l1, l2, l3);   /* jal 80029300, delay addiu a0,zero,7 */
        l0 = m->exit_reg(m->ctx, 4u);                    /* $a0..$a3 as 80029300 left them */
        l1 = m->exit_reg(m->ctx, 5u);
        l2 = m->exit_reg(m->ctx, 6u);
        l3 = m->exit_reg(m->ctx, 7u);
        r = m->call4(m->ctx, 0x8001F0A8u, l0, l1, l2, l3); /* jal 8001f0a8, delay nop */
    }

    m->gpr_set(m->ctx, 31u, RD32(sp + 16u));           /* lw ra,0x10(sp) */
    m->sp_set(m->ctx, sp + 24u);
    return r;
}
