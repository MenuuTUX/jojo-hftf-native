#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800BC20C(const GuestMem *m, uint32_t a0) {
    uint32_t s0;                            /* pointer into byte_800CBA0F, walks down */
    uint32_t s1 = 0x800DAE50u;              /* dword_800DAE50, written byte by byte */
    uint32_t s2 = 0x04000000u;
    uint32_t s3;
    uint32_t r;

    /* 800BC218..800BC240: clamp a0 to 0x5F5E0FF (sltu + bne, delay slot sets v1 = a0 otherwise) */
    if (a0 > 0x05F5E0FFu) {
        s3 = 0x05F5E0FFu;
    } else {
        s3 = a0;
    }

    WR32(0x800DAE50u, 0u);                  /* sw zero at 800BC24C */
    s0 = 0x800CBA0Fu;                       /* 800BC258..800BC260 */

    do {
        if ((s3 & s2) != 0u) {
            WR8(0x800DAE58u, 0u);           /* sb zero at 800BC274 */

            r = m->call4(m->ctx, 0x800BC19Cu, RD8(s1 + 0u), RD8(s0), 0u, 0u);
            s0 -= 1u;
            WR8(s1 + 0u, r);

            r = m->call4(m->ctx, 0x800BC19Cu, RD8(s1 + 1u), RD8(s0), 0u, 0u);
            s0 -= 1u;
            WR8(s1 + 1u, r);

            r = m->call4(m->ctx, 0x800BC19Cu, RD8(s1 + 2u), RD8(s0), 0u, 0u);
            s0 -= 1u;
            WR8(s1 + 2u, r);

            r = m->call4(m->ctx, 0x800BC19Cu, RD8(s1 + 3u), RD8(s0), 0u, 0u);
            s0 -= 1u;
            WR8(s1 + 3u, r);
        } else {
            s0 -= 4u;                       /* 800BC2C8 */
        }
        s2 >>= 1;
    } while (s2 != 0u);

    return RD32(0x800DAE50u);               /* lw v0,-0x51b0(v0) at 800BC2DC */
}
