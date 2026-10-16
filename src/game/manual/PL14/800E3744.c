#include "manual.h"
#include "hex2c_rt.h"

/* PL14 800E3744..800E3818 (slot A, base 800DF000). $a0 = P (kept in $s0). $a1 is loaded (lw a1,-0xfec(v0)) before any read,
 * so it is a dummy parameter (nargs 4 keeps $a2 and $a3 as parameters 3 and 4).
 * Stores: +7 = 5; +0x78 = 0; +0x70 = 0; +0x74 = table word 0x800DF014; +0x7C = word 0x800DF018 (delay slot of the first jal).
 * jal 800afd84 with $a0 = P, $a1 = word 0x800DF014, $a2/$a3 entry values.
 * Then the byte +0x1F8 selects a table pair: 0 -> words 0x800DF01C/0x800DF020 to +0x70/+0x78; 1 -> words 0x800DF024/0x800DF028;
 * other values -> none. If byte +0x0B is non-zero, +0x70 is negated, and +0x78 too when +0x0B is still non-zero.
 * jal 800268bc with $a0 = P, $a1 = word 0x800E8C64, and $a2/$a3 as the 800afd84 callee left them. Return is its $v0.
 * The delay slot of each branch (`addiu v0,zero,1` and `lui v0,0x800e`) only sets $v0 and does not change the paths. */
uint32_t man_PL14_800E3744(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;
    uint32_t ld_a1, ld_v1, v;
    (void)a1;
    ld_a1 = RD32(0x800DF014u);                          /* lui v0,0x800e; lw a1,-0xfec(v0) (800e3758) */
    ld_v1 = RD32(0x800DF018u);                          /* lw v1,4(v1) (800e375c) */
    WR8(p + 7u, 5u);                                    /* addiu v0,zero,5; sb v0,7(s0) */
    WR32(p + 0x78u, 0u);
    WR32(p + 0x70u, 0u);
    WR32(p + 0x74u, ld_a1);
    WR32(p + 0x7Cu, ld_v1);                             /* delay slot of jal 800afd84 */
    m->call4(m->ctx, 0x800AFD84u, p, ld_a1, a2, a3);
    a2 = m->gpr_get(m->ctx, 6u);                        /* $a2 as the callee left it */
    a3 = m->gpr_get(m->ctx, 7u);

    v = RD8(p + 0x1F8u);                                /* lbu v1,0x1f8(s0) (800e3780) */
    if (v == 0u) {                                      /* bne v1,zero,800e37ac not taken */
        WR32(p + 0x70u, RD32(0x800DF01Cu));             /* lw v0,-0xfe4(v0); lw v1,4(v1) at 0x800DF01C/0x800DF020 */
        WR32(p + 0x78u, RD32(0x800DF020u));             /* j 800e37c8; delay sw v1,0x78(s0) */
    } else if (v == 1u) {                               /* bne v1,v0 with v0 = 1 not taken */
        WR32(p + 0x70u, RD32(0x800DF024u));            /* lw v1,8(v0) with v0 = 0x800DF01C */
        WR32(p + 0x78u, RD32(0x800DF028u));            /* lw v0,0xc(v0) */
    }

    v = RD8(p + 0xBu);                                  /* lbu v0,0xb(s0) (800e37c8) */
    if (v != 0u) {                                      /* beq v0,zero,800e3800 not taken */
        WR32(p + 0x70u, 0u - RD32(p + 0x70u));          /* lw v0,0x70; subu v0,zero,v0; sw v0,0x70 (delay of beq 800e37e4) */
        if (RD8(p + 0xBu) != 0u)                        /* lbu v1,0xb(s0); beq v1,zero,800e37fc not taken */
            WR32(p + 0x78u, 0u - RD32(p + 0x78u));      /* lw v0,0x78; subu; sw v0,0x78 */
    }

    v = RD32(0x800E8C64u);                              /* lui v0,0x800f; lw a1,-0x739c(v0) (800e3800) */
    return m->call4(m->ctx, 0x800268BCu, p, v, a2, a3); /* jal 800268bc, delay addu a0,s0,zero */
}
