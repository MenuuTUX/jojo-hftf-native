#include "manual.h"
#include "hex2c_rt.h"

/* 80010948: p = RD32(0x80066294); state 0 stored at p+0 (in the delay slot of EnterCriticalSection, so before it).
 * EnterCriticalSection(a0) passes the incoming a0 on unchanged. Then the record pointer is re-read, h = RD32(p+4) is the
 * a0 for CloseTh and for ExitCriticalSection (no instruction changes $a0 between them). Ends with ChangeTh(0xFF000000).
 * Returns ChangeTh's $v0. */
uint32_t man_80010948(const GuestMem *m, uint32_t a0) {
    uint32_t p, h;
    p = RD32(0x80066294u);
    WR16(p, 0u);                                                                       /* sh zero,0x0(v0), delay slot */
    m->call4(m->ctx, 0x80035A70u, a0, 0, 0, 0);                                        /* EnterCriticalSection(a0) */
    p = RD32(0x80066294u);                                                             /* lw v0,0x6294(s0) after the call */
    h = RD32(p + 4u);                                                                  /* lw a0,0x4(v0) */
    m->call4(m->ctx, 0x80035A50u, h, 0, 0, 0);                                         /* CloseTh(h) */
    m->call4(m->ctx, 0x80035A80u, h, 0, 0, 0);                                         /* ExitCriticalSection(h) */
    return m->call4(m->ctx, 0x80035A60u, 0xFF000000u, 0, 0, 0);                        /* ChangeTh(0xFF000000) */
}
