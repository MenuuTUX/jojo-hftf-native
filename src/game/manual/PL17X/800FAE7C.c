#include "manual.h"
#include "hex2c_rt.h"

#define S8_(x) ((uint32_t)(int32_t)(int8_t)(x))

/* 800FAE7C (PL17X, slot B, 44 bytes) has no jr/j: its last instruction (lbu v0,0xb(v1) at 800faea4) falls through into
 * 800FAEA8 (160 bytes, IDA split it as a separate routine). This function is the whole routine, 800FAE7C..800FAF44: the
 * fall-through part is written inline with $v0 and $v1 as the first part leaves them. The generated C called 800FAEA8
 * through call4, which drops $v0 (the lbu of 0xb) and $v1 (the struct pointer). $a0 = P. $a1 is overwritten (a1 = 168 or
 * 192) before the jal and is a dummy parameter; $a2 and $a3 are passed unchanged to the jal 800268bc (nargs 4). The jal
 * takes $a1 = word at 0x800FD12C. $v0 at jr ra is the callee's $v0. */
uint32_t man_PL17X_800FAE7C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;                                /* a0 is not written in this routine */
    uint32_t v0, v1;

    /* 800fae7c: part 1 */
    v1 = RD32(p + 0x80u);                           /* lw v1,0x80(a0) */
    WR8(p + 0xB8u, 0u);                             /* sb zero,0xb8(a0) */
    v0 = RD8(v1 + 0xB1u);                           /* lbu v0,0xb1(v1) */
    WR8(p + 0xB0u, 0u);                             /* sb zero,0xb0(a0) */
    WR8(p + 0xB1u, v0);                             /* sb v0,0xb1(a0) */
    v0 = RD8(v1 + 0x136u);                          /* lbu v0,0x136(v1) */
    WR8(p + 0x100u, v0);                            /* sb v0,0x100(a0) */
    v0 = RD8(v1 + 0x0Bu);                           /* lbu v0,0xb(v1): last instruction of part 1 */

    /* 800faea8: part 2, reached by falling through */
    WR8(p + 0x0Bu, v0);                             /* sb v0,0xb(a0): delay slot of nop */
    v0 = RD8(v1 + 0x32u);                           /* lbu v0,0x32(v1) */
    v0 = v0 - 1u;                                   /* addiu v0,v0,-1 */
    WR8(p + 0x32u, v0);                             /* sb v0,0x32(a0) */
    v0 = S8_(RD8(v1 + 0x136u));                     /* lb v0,0x136(v1) */
    a1 = (v0 == 0u) ? 168u : 192u;                  /* beq v0,zero,800faed4 with addiu a1,zero,168 in its delay slot */
    v0 = 1u;                                        /* addiu v0,zero,1 */
    WR8(p + 0x01u, v0);                             /* sb v0,1(a0) */
    v0 = 0x80u;                                     /* addiu v0,zero,128 */
    WR8(p + 0xE8u, v0);                             /* sb v0,0xe8(a0) */
    WR8(p + 0xE4u, v0);                             /* sb v0,0xe4(a0) */
    WR8(p + 0xE0u, v0);                             /* sb v0,0xe0(a0) */
    v0 = 63u;                                       /* addiu v0,zero,63 */
    WR16(p + 0x1Eu, a1);                            /* sh a1,0x1e(a0) */
    WR16(p + 0xF0u, v0);                            /* sh v0,0xf0(a0) */
    WR16(p + 0xEEu, v0);                            /* sh v0,0xee(a0) */
    WR16(p + 0x6Au, RD16(v1 + 0x6Au));              /* lhu v0,0x6a(v1); sh v0,0x6a(a0) */
    WR16(p + 0x6Eu, RD16(v1 + 0x6Eu));              /* lhu v0,0x6e(v1); sh v0,0x6e(a0) */
    v0 = RD32(v1 + 0x12Cu);                         /* lw v0,0x12c(v1) */
    v0 = RD32(v0 + 0x04u);                          /* lw v0,4(v0) */
    WR32(p + 0x48u, v0);                            /* sw v0,0x48(a0) */
    a1 = RD32(0x800FD12Cu);                         /* lui v0,0x8010; lw a1,-0x2ed4(v0) */
    v0 = m->call4(m->ctx, 0x800268BCu, p, a1, a2, a3);   /* jal 800268bc, delay slot nop */
    return v0;                                      /* epilogue returns the callee's $v0 */
}
