#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800BC51C(const GuestMem *m, uint32_t a0, uint32_t a1) {
    uint32_t p = a0;
    uint32_t s0 = a1;                       /* short value, stored with sh and added with addu */
    uint32_t v;
    int32_t x30, x2e;

    m->call4(m->ctx, 0x800BC608u, p, 0x10000000u, 0, 0);   /* lui a1,0x1000 is in the delay slot of jal */

    v = RD32(p + 0x130);
    if (RD8(v + 0x36) == 0) {               /* bne at 800bc54c not taken */
        WR16(v + 0x30, s0);                 /* sh s0,0x30(v1) */
        WR16(v + 0x2C, s0);                 /* sh s0,0x2c(v1): delay slot of j 800bc5a8 */
    } else {
        WR16(v + 0x2C, RD16(v + 0x2C) + s0);            /* 800bc560..800bc56c */
        v = RD32(p + 0x130);
        WR16(v + 0x30, RD16(v + 0x30) + s0);            /* 800bc578..800bc584 */
        v = RD32(p + 0x130);
        if (RDS16(v + 0x2C) >= 145) {       /* slti v0,v0,145 ; bne skips the store when < 145 */
            WR16(v + 0x2C, 144);            /* sh v0,0x2c(v1) at 800bc5a4 */
        }
    }
    v = RD32(p + 0x130);                    /* lw v1,0x130(s1) at 800bc5a8 */
    x30 = RDS16(v + 0x30);
    x2e = RDS16(v + 0x2E);
    if (x2e < x30) {                        /* slt v0,v0,a0 ; beq skips when not less */
        if (x30 >= 1000) {                  /* slti v0,a0,1000 ; bne skips the store when < 1000 */
            WR16(v + 0x30, 999);            /* sh v0,0x30(v1) at 800bc5d0 */
        }
        v = RD32(p + 0x130);
        WR16(v + 0x2E, RD16(v + 0x30));     /* lhu v0,0x30(v1) ; sh v0,0x2e(v1) */
    }
    v = RD32(p + 0x130);
    WR8(v + 0x36, 1);                       /* sb v0,0x36(v1) at 800bc5f0, v0 = 1 */
    return 1;
}
