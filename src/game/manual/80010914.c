#include "manual.h"
#include "hex2c_rt.h"

/* 80010914: current thread record p = RD32(0x80066294). Stores the incoming a0 (low 16 bits) at p+2, then state 1 at p+0
 * (both before the call: the second sh sits in the jal's delay slot), then ChangeTh(0xFF000000). Returns ChangeTh's $v0. */
uint32_t man_80010914(const GuestMem *m, uint32_t a0) {
    uint32_t p;
    p = RD32(0x80066294u);
    WR16(p + 2u, a0);                                                                  /* sh a0,0x2(v1) */
    WR16(p, 1u);                                                                       /* sh v0,0x0(v1), delay slot */
    return m->call4(m->ctx, 0x80035A60u, 0xFF000000u, 0, 0, 0);                        /* ChangeTh(0xFF000000) */
}
