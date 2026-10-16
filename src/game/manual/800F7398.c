#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800F7428(const GuestMem *m, uint32_t a0);

/* 800F7398: if the byte at a0+8 is nonzero, tail-call 800F7428 (jal, then return its $v0); else call 800F73D8. */
uint32_t man_800F7398(const GuestMem *m, uint32_t a0) {
    if (RD8(a0 + 8u) != 0) return man_800F7428(m, a0);
    return m->call4(m->ctx, 0x800F73D8u, a0, 0, 0, 0);
}
