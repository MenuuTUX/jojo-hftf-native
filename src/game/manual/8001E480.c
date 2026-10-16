#include "manual.h"
#include "hex2c_rt.h"

/* 8001E480: a0 = 0x800DF000 (lui 0x800d / ori 0xf000), a1 = 819 (addiu zero,819), then jal 8001D984.
 * $a2 and $a3 are not written by the original, so they pass on unchanged. Returns the $v0 of 8001D984.
 * The pseudocode's jpt_800E052C is not the first argument. */
uint32_t man_8001E480(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    return m->call4(m->ctx, 0x8001D984u, 0x800DF000u, 819u, a2, a3);
}
