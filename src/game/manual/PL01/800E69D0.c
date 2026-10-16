#include "manual.h"
#include "hex2c_rt.h"

/* PL01 800E69D0..800E6A74 (164 bytes, slot A base 800DF000). Let R = RD32(a0+0x80). Writes a0+0xB8 = 0x80, a0+0x108 = 0,
 * a0+1 = 0. Then d = RDS8(0x800EA0DC + RD8(a0+4)); a0+0x6A = RD16(R+0x6A) + d if RD8(a0+0xB) != 0, else - d;
 * a0+0x6E = RD16(R+0x6E) + 64. Finally jal 800268BC(a0, RD32(0x800EDC94), a2, a3) and returns its $v0.
 * The generated C read RD32(0x800EFEB8); the lui v0,0x800f / lw a1,-0x236c(v0) pair addresses 0x800EDC94.
 * $a2 and $a3 pass through unchanged; $a1 is set by the lw before any use. */
uint32_t man_PL01_800E69D0(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t flag, r, v1, arg1;
    int32_t d;

    (void)a1;                                       /* $a1 is loaded from 0x80(a0) before any use */
    WR8(a0 + 0xB8u, 0x80u);                         /* addiu v0,zero,128; sb v0,0xb8(a0) */
    flag = RD8(a0 + 0x0Bu);                         /* lbu v0,0xb(a0) */
    r = RD32(a0 + 0x80u);                           /* lw a1,0x80(a0) */
    WR32(a0 + 0x108u, 0u);                          /* sw zero,0x108(a0) */
    WR8(a0 + 0x01u, 0u);                            /* sb zero,0x1(a0) (beq delay slot) */
    d = (int32_t)RDS8(0x800EA0DCu + RD8(a0 + 0x04u));   /* lbu v1,4(a0); table at 0x800EA0DC; signed byte in $v0 */
    v1 = RD16(r + 0x6Au);                           /* lhu v1,0x6a(a1) */
    if (flag != 0u) {                               /* bne path: addu v1,v1,v0 */
        v1 = v1 + (uint32_t)d;
    } else {                                        /* subu v1,v1,v0 */
        v1 = v1 - (uint32_t)d;
    }
    WR16(a0 + 0x6Au, v1);                           /* sh v1,0x6a(a0) */
    WR16(a0 + 0x6Eu, RD16(r + 0x6Eu) + 64u);       /* lhu v0,0x6e(a1); addiu v0,v0,64; sh v0,0x6e(a0) */
    arg1 = RD32(0x800EDC94u);                       /* lui v0,0x800f; lw a1,-0x236c(v0) */
    return m->call4(m->ctx, 0x800268BCu, a0, arg1, a2, a3);   /* jal 800268bc; delay nop */
}
