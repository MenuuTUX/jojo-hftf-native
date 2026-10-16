#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800C6060(const GuestMem *m, uint32_t a0) {
    uint32_t s0 = a0;
    uint32_t a4 = RD32(s0 + 0x18);          /* lw a0,0x18(s0) at 800c6070 */
    uint32_t v0, r, a1v = 0, b3;
    int32_t v1;

    WR16(s0 + 0x16, RD16(a4 + 0x16) + 4);
    v1 = RDS8(0x80066C6Cu);
    if (v1 != 0 && v1 != 5 && ((int32_t)RDS8(0x8006CE51u) == 3 || v1 == 3)) {
        WR16(s0 + 0xE, 15);                 /* addiu v1,zero,15 ; sh v1,0xe(s0) */
        v0 = RD16(a4 + 0x12) - 8u;          /* lhu v0,0x12(a0) ; addiu v0,v0,-8 */
        WR16(s0 + 0x12, v0);                /* sh v0,0x12(s0): delay slot of j 800c6134 */
        return v0;
    }
    /* addiu a1,zero,3 at 800c60a8 runs whenever v1 is neither 0 nor 5 */
    if (v1 != 0 && v1 != 5) {
        a1v = 3;
    }
    b3 = RD8(s0 + 3);                       /* lbu v0,0x3(s0) at 800c60d4 */
    if (b3 != 0) {
        WR16(s0 + 0x12, RD16(a4 + 0x12));   /* lhu v0,0x12(a0) ; sh v0,0x12(s0) at 800c6114 */
    } else {
        WR16(s0 + 0x12, RD16(a4 + 0x12) - 8u);   /* addiu -8 ; sh at 800c60f0 */
    }
    /* lui v0,0x8007 ; lb a0,-0x2d20(v0): byte at 0x8006D2E0 */
    r = m->call4(m->ctx, 0x800BC20Cu, (uint32_t)(int32_t)RDS8(0x8006D2E0u), a1v, 0, 0);
    if (b3 != 0) {
        v0 = (r & 0xFu) + 5u;               /* andi v0,v0,0xf ; addiu v0,v0,5 */
    } else {
        v0 = ((r >> 4) & 0xFu) + 5u;        /* srl v0,v0,4 in the delay slot of j 800c6128 */
    }
    WR16(s0 + 0xE, v0);                     /* sh v0,0xe(s0) at 800c6130 */
    return v0;
}
