#include "manual.h"
#include "hex2c_rt.h"

/* PL07X 800FA410..800FA4A8 (152 bytes, slot B base 800F4800). Let R = RD32(a0+0x80). Writes a0+0x1D = 0xFF, a0+0x1E =
 * 24*RDS8(R+0x136)+219, a0+0x74 = 0xFFFC8000, a0+0x6E = RD16(R+0x6E)+58, and a0+0x72 / a0+0x6A from RD16(R+0x6A)
 * (sign of the + or -58 depends on RD8(a0+0xB)). Then jal 800268BC(a0, RD32(0x800FB45C), a0, a3): the MIPS sets $a2 = a0
 * at entry (addu a2,a0,zero), so the call passes $a2 = a0, not the caller's $a2. Returns the callee's $v0.
 * The generated C read RD32(0x800FDE1C); the lui v0,0x8010 / lw a1,-0x4ba4(v0) pair addresses 0x800FB45C. */
uint32_t man_PL07X_800FA410(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t r, w, v0, v1, arg1;

    (void)a1;                                       /* $a1 is loaded from 0x80(a0) before any read */
    (void)a2;                                       /* $a2 is overwritten by addu a2,a0,zero */
    r = RD32(a0 + 0x80u);                           /* lw a1,0x80(a2) */
    WR8(a0 + 0x1Du, 0xFFu);                         /* addiu v0,zero,-1; sb v0,0x1d(a2) */
    WR16(a0 + 0x1Eu, (uint32_t)(24 * (int32_t)RDS8(r + 0x136u) + 219));   /* lb v1,0x136(a1); sll/addu x24; +219; sh */
    w = RD16(r + 0x6Eu);                            /* lhu v0,0x6e(a1) */
    v1 = RD8(a0 + 0x0Bu);                           /* lbu v1,0xb(a2) */
    WR32(a0 + 0x74u, 0xFFFC8000u);                  /* lui a0,0xfffc; ori a0,a0,0x8000; sw a0,0x74(a2) */
    WR16(a0 + 0x6Eu, w + 58u);                      /* addiu v0,v0,58; sh v0,0x6e(a2) */
    if (v1 != 0u) {                                 /* bne v1,zero,800fa474 */
        v0 = RD16(r + 0x6Au);                       /* lhu v0,0x6a(a1) */
        WR16(a0 + 0x72u, 58u);                      /* addiu v1,zero,58; sh v1,0x72(a2) */
        WR16(a0 + 0x6Au, v0 + 58u);                 /* addiu v0,v0,58; sh v0,0x6a(a2) */
    } else {
        v0 = RD16(r + 0x6Au);                       /* 800fa460: lhu v0,0x6a(a1) */
        WR16(a0 + 0x72u, 0xFFC6u);                  /* addiu v1,zero,-58; sh v1,0x72(a2) */
        WR16(a0 + 0x6Au, v0 - 58u);                 /* j 800fa484; delay addiu v0,v0,-58; sh v0,0x6a(a2) */
    }
    arg1 = RD32(0x800FB45Cu);                       /* lui v0,0x8010; lw a1,-0x4ba4(v0) */
    return m->call4(m->ctx, 0x800268BCu, a0, arg1, a0, a3);   /* jal 800268bc; delay addu a0,a2,zero (a0 = P) */
}
