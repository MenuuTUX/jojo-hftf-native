#include "manual.h"
#include "hex2c_rt.h"

/* 800F5800: reads $v0 at entry. Nonzero $v0 skips the 800F5AD0 test. Each test returns nonzero to stop
   the chain; the last call's $v0 is returned. */
uint32_t man_800F5800(const GuestMem *m, uint32_t a0, uint32_t in_v0) {
    uint32_t r;
    if (in_v0 == 0) {
        r = m->call4(m->ctx, 0x800F5AD0u, a0, 0, 0, 0);
        if (r != 0) return r;
    }
    r = m->call4(m->ctx, 0x800F5984u, a0, 0, 0, 0);
    if (r != 0) return r;
    r = m->call4(m->ctx, 0x800F58E8u, a0, 0, 0, 0);
    if (r != 0) return r;
    r = m->call4(m->ctx, 0x800F5A38u, a0, 0, 0, 0);
    if (r != 0) return r;
    r = m->call4(m->ctx, 0x800F5938u, a0, 0, 0, 0);
    if (r != 0) return r;
    r = m->call4(m->ctx, 0x800F59D4u, a0, 0, 0, 0);
    if (r != 0) return r;
    r = m->call4(m->ctx, 0x800F589Cu, a0, 0, 0, 0);
    if (r != 0) return r;
    return m->call4(m->ctx, 0x800F5A84u, a0, 0, 0, 0);
}
