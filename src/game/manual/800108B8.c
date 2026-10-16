#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800108B8(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);

/* 800108B8: sets up thread slot a0 (slot at 0x801FC400 + a0 * 128) with the function in a1. Stores 2 (ready) at slot+0,
 * EnterCriticalSection, OpenTh-style BIOS call B0 0x0E with (a1, slot+8, slot+0x10, a3), stores its result at slot+4,
 * then ExitCriticalSection (syscall 2). The original returns the $v0 it has at jr ra: syscall 2 preserves $v0 (it has no
 * return value), so that is the OpenTh result, not anything Exit returns. The C therefore returns the OpenTh result.
 * a2 is only passed on unchanged to the first call, a3 to the first two calls. Runs with $sp lowered by the original's 32-byte frame. */
uint32_t man_800108B8(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx) - 0x20u;
    uint32_t s0, r, t1, t2;
    m->sp_set(m->ctx, sp);
    s0 = 0x801FC400u + (a0 << 7);
    WR16(s0, 2);
    m->call4(m->ctx, 0x80035A70u, a0, a1, a2, a3);                         /* EnterCriticalSection (stub sets $a0 itself) */
    t1 = RD32(s0 + 8u);                                                    /* lw a1,8(s0) */
    t2 = RD32(s0 + 0x10u);                                                 /* lw a2,0x10(s0) */
    r = m->call4(m->ctx, 0x80035A40u, a1, t1, t2, a3);                     /* B0 0x0E (OpenTh) */
    WR32(s0 + 4u, r);
    m->call4(m->ctx, 0x80035A80u, a1, t1, t2, a3);                         /* ExitCriticalSection (stub sets $a0 = 2; $v0 kept) */
    m->sp_set(m->ctx, sp + 0x20u);
    return r;
}
