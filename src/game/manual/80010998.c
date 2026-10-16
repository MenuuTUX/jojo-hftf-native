#include "manual.h"
#include "hex2c_rt.h"

/* 80010998: slot s = 0x801FC400 + (a0 << 7). If the halfword at s is 0, returns 0 ($v0 = that halfword). Otherwise: halfword
 * at s set to 0 (delay slot of the first jal, so before it), EnterCriticalSection with $a0 = a0 << 7 (the sll result is still
 * in $a0 at that jal), h = RD32(s+4) is the a0 for CloseTh and ExitCriticalSection. No ChangeTh here. CloseTh's $v0 is the
 * function result: syscall 2 (ExitCriticalSection) leaves it unchanged. GuestMem.call4 returns the guest result separately and
 * restores the caller's register file, so retain CloseTh's result across that final call. */
uint32_t man_80010998(const GuestMem *m, uint32_t a0) {
    uint32_t a, s, h, v;
    a = a0 << 7;
    s = 0x801FC400u + a;
    v = RD16(s);
    if (v == 0u) {
        return v;
    }
    WR16(s, 0u);
    m->call4(m->ctx, 0x80035A70u, a, 0, 0, 0);                                        /* EnterCriticalSection(a) */
    h = RD32(s + 4u);                                                                  /* lw a0,0x4(s0) */
    v = m->call4(m->ctx, 0x80035A50u, h, 0, 0, 0);                                     /* CloseTh(h) */
    m->call4(m->ctx, 0x80035A80u, h, 0, 0, 0);                                         /* ExitCriticalSection(h), syscall 2 preserves $v0 */
    return v;
}
