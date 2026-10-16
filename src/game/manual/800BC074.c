#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800BC074(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2) {
    uint32_t p = a0;
    uint32_t s1 = 0x80066CA0u;              /* lui v0,0x8006; addiu s1,v0,0x6CA0 */
    uint32_t a4 = RD32(p + 0x170);          /* lw a0,0x170(s0) at 800bc0ac */
    uint32_t s3, v0;

    if (RD8(s1 + 0x2A) != (uint32_t)(int32_t)RDS8(p + 0x136)) {
        WR8(s1 + 0x29, 0);                  /* sb zero,0x29(s1) at 800bc0b8 */
    }
    s3 = a1;                                /* addu s3,s2,zero (delay slot of beq at 800bc0b0) */
    WR8(p + 0x107, 12);
    WR8(a4 + 0x107, 12);
    WR8(s1 + 0x10, RD8(p + 0x136) + 1);     /* sb v0,0x10(s1): delay slot of jal 80025354 */
    m->call4(m->ctx, 0x80025354u, 6, a1, a2, 0);
    if (RD8(p + 0x0B) != 0) {
        s3 = 0u - a1;                       /* subu s3,zero,s2 at 800bc0ec */
    }
    WR16(p + 0x102, RD16(p + 0x6A) + s3);
    WR16(p + 0x104, RD16(p + 0x6E) + a2);   /* sh v1,0x104(s0): delay slot of jal 800c73f8 */
    m->call4(m->ctx, 0x800C73F8u, p, a1, a2, 0);
    WR8(s1 + 0x11, 30);                     /* sb s2,0x11(s1): delay slot of jal 8002438c */
    m->call4(m->ctx, 0x8002438Cu, 122, a1, a2, 0);
    v0 = (uint32_t)(int32_t)RDS8(s1 + 0x24);
    if (v0 != 0) {
        return v0;                          /* bne v0,zero,800bc17c: returns the byte at 0x80066CC4 */
    }
    if (RD8(p + 0x202) != 0) {
        WR8(s1 + 0x11, 40);                 /* delay slot of bne at 800bc134 sets $v0 = 40 */
        m->call4(m->ctx, 0x800C7458u, p, 1, 0, 0);
    } else {
        WR8(s1 + 0x11, 30);
        m->call4(m->ctx, 0x800C7458u, p, 0, 0, 0);
    }
    v0 = m->call4(m->ctx, 0x80025548u, 6, 4, 0, 0);
    if (v0 != 0) {
        WR32(v0 + 0x80, p);                 /* sw s0,0x80(v0) at 800bc178 */
    }
    return v0;
}
