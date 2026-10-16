#include "manual.h"
#include "hex2c_rt.h"

/* 8002816C (TEXT). Calls through the table at 0x8005A938 (4-byte entries), indexed by the byte that the word at
 * 0x8005A930 points to. $a0..$a3 are not changed here: they are passed on unchanged to the callee, which reads them
 * (nargs 4). Returns the callee's $v0. */
uint32_t man_8002816C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t fn = RD32(0x8005A938u + 4u * (uint32_t)RD8(RD32(0x8005A930u)));

    return m->calli(m->ctx, fn, a0, a1, a2, a3);
}
