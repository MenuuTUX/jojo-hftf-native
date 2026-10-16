#include "manual.h"
#include "hex2c_rt.h"

/* Falls through into sub_800A29A0 (reads $v0 at entry, from lb at 800A299C). Its body is inlined:
 * $v0 == 0 tail-calls 800E72B0; otherwise dispatches through the table at 800A3610, then returns 800A2B68's $v0. */
uint32_t man_800A2998(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t idx;

    if (RD8(0x8006CE56u) == 0u) {
        return m->call4(m->ctx, 0x800E72B0u, a0, a1, a2, a3);
    }
    idx = RD16(a0 + 2u);
    (void)m->calli(m->ctx, RD32(0x800A3610u + idx * 4u), a0, a1, a2, a3);
    return m->call4(m->ctx, 0x800A2B68u, a0, a1, 0u, 0u);
}
