#include "manual.h"
#include "hex2c_rt.h"

/* 8001E784 (TEXT). Tail of the call: 8001D984(0x800DF000, 762). The constant is lui 0x800D + ori 0xF000 (not a
 * memory read of jpt_800E052C). $a2/$a3 are never written before the jal and pass on unchanged (nargs 4).
 * Returns the $v0 that 8001D984 leaves. */
uint32_t man_8001E784(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    return m->call4(m->ctx, 0x8001D984u, 0x800DF000u, 762u, a2, a3);
}
