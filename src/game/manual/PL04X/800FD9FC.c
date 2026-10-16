#include "manual.h"
#include "hex2c_rt.h"

/* 800FD9FC (PL04X, slot B). $a0 = P (entity struct); $a1 is overwritten (lw a1) before any read, so it is a
 * dummy parameter; $a2 and $a3 are never written and are passed on unchanged to the jal 800268bc (nargs 4).
 * The generated C dereferenced the table bases 0x80100004 and 0x80100010 (RD32 of them): the MIPS builds those
 * with lui+addiu, so they are the table addresses themselves and the bytes are read with lbu/lb at base + 2*idx. */
uint32_t man_PL04X_800FD9FC(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;                                    /* s0 */
    uint32_t s1 = RD32(p + 0x80u);                      /* lw s1,0x80(s0) */
    uint32_t a0s = RD32(s1 + 0x15Cu);                   /* lw a0,0x15c(s1), at fda18 (unconditional) */
    uint32_t v0, v1, idx;

    WR16(p + 0xF0u, 31u);                               /* sh v0,0xf0(s0) */
    WR16(p + 0xEEu, 31u);                               /* sh v0,0xee(s0) */
    WR16(p + 0x1Eu, RD8(s1 + 2u) == 0 ? 8u : 32u);      /* beq + delay slot addiu v1 (both paths) */
    WR8(p + 0xB0u, 133u);                               /* sb v0,0xb0(s0) */
    WR8(p + 0xB8u, 160u);                               /* sb v0,0xb8(s0) */

    v1 = RD8(p + 4u);                                   /* lbu v1,4(s0) */
    if (v1 == 2u)                                       /* bne v1,v0(=2) not taken */
        v0 = RD8(s1 + 0x32u) - 1u;                      /* lbu v0,0x32(s1); addiu v0,v0,-1 */
    else                                                /* bne taken: fda6c */
        v0 = RD8(a0s + 0x32u) - 2u;                     /* lbu v0,0x32(a0); addiu v0,v0,-2 */
    WR8(p + 0x32u, v0);                                 /* sb v0,0x32(s0) */

    /* lui v0,0x8010; lw a1,0x7fc4(v0): a1 = word at 0x80107FC4 (a real load). $a2, $a3 pass on. */
    m->call4(m->ctx, 0x800268BCu, p, RD32(0x80107FC4u), a2, a3);

    if (RD8(s1 + 0x0Bu) != 0) {                         /* lbu v0,0xb(s1); beq v0,zero not taken */
        idx = RD8(p + 4u);
        v1 = RD16(s1 + 0x6Au) - (uint32_t)(int32_t)RDS8(0x80100004u + 2u * idx);
        WR16(p + 0x6Au, v1);                            /* sh v1,0x6a(s0) */
        v0 = RD8(0x80100010u + 2u * idx);               /* lbu v0,0(v0) at table 0x80100010 */
        WR16(p + 0x72u, v0);                            /* delay slot of j fdb30: sh v0,0x72(s0) */
    } else {
        idx = RD8(p + 4u);
        v1 = RD16(s1 + 0x6Au) + (uint32_t)(int32_t)RDS8(0x80100004u + 2u * idx);
        WR16(p + 0x6Au, v1);                            /* sh v1,0x6a(s0) */
        v0 = 0u - RD8(0x80100010u + 2u * idx);          /* subu v0,zero,v0 */
        WR16(p + 0x72u, v0);                            /* sh v0,0x72(s0) */
    }

    idx = RD8(p + 4u);
    v1 = RD16(s1 + 0x6Eu) + (uint32_t)(int32_t)RDS8(0x80100004u + 2u * idx + 1u);
    WR16(p + 0x6Eu, v1);                                /* sh v1,0x6e(s0) */

    v0 = RD8(0x80100010u + 2u * idx + 1u);              /* lbu v0,1(v0) */
    WR16(p + 0x0Cu, v0);                                /* sh v0,0xc(s0) */
    return v0;                                          /* $v0 at jr ra */
}
