#include "manual.h"
#include "hex2c_rt.h"

/* 800109F0: p = RD32(0x80066294). Stores state 3 at p+0 and the incoming a0 at p+0xC (both before the call: the sw is in
 * the delay slot of EnterCriticalSection). EnterCriticalSection(a0) passes a0 on unchanged. The record pointer is re-read,
 * h = RD32(p+4) is the a0 for CloseTh and ExitCriticalSection. Ends with ChangeTh(0xFF000000). Returns ChangeTh's $v0. */
uint32_t man_800109F0(const GuestMem *m, uint32_t a0) {
    uint32_t p, h;
    p = RD32(0x80066294u);
    WR16(p, 3u);                                                                       /* sh v0,0x0(v1) */
    WR32(p + 0xCu, a0);                                                                /* sw a0,0xc(v1), delay slot */
    m->call4(m->ctx, 0x80035A70u, a0, 0, 0, 0);                                        /* EnterCriticalSection(a0) */
    p = RD32(0x80066294u);                                                             /* lw v0,0x6294(s0) after the call */
    h = RD32(p + 4u);                                                                  /* lw a0,0x4(v0) */
    m->call4(m->ctx, 0x80035A50u, h, 0, 0, 0);                                         /* CloseTh(h) */
    m->call4(m->ctx, 0x80035A80u, h, 0, 0, 0);                                         /* ExitCriticalSection(h) */
    return m->call4(m->ctx, 0x80035A60u, 0xFF000000u, 0, 0, 0);                        /* ChangeTh(0xFF000000) */
}
