#include "manual.h"
#include "hex2c_rt.h"

/* 80010FD8 (TEXT). Calls 80011020(0, 0x8006CE68, 0x8006CEB8) and then 80011020(16, 0x8006CE90, 0x8006CEC8), and returns
 * the $v0 of the second call. Neither call sets $a3, and 80011020 reads it, so $a3 is passed on as received (nargs 4).
 * $a0..$a2 are overwritten before any read, so they are not used. */
uint32_t man_80010FD8(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)a0;
    (void)a1;
    (void)a2;
    m->call4(m->ctx, 0x80011020u, 0u, 0x8006CE68u, 0x8006CEB8u, a3);
    return m->call4(m->ctx, 0x80011020u, 16u, 0x8006CE90u, 0x8006CEC8u, a3);
}
