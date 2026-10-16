#include "manual.h"
#include "hex2c_rt.h"

/* The lb at 800BB34C loads a signed byte, not the address of byte_80066CC4.
 * a2 becomes the actor pointer; a3 is passed through to 800BB490. */
uint32_t man_800BB30C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t result = RD8(0x80066CA2u), sp, table, index;
    if (result) return result;
    result = RD8(a0 + 0x201u);
    if (result) return result;
    result = RD8(a0 + 0x203u);
    if (result) return result;
    result = (uint32_t)RDS8(0x80066CC4u);
    if (result) return result;
    index = RD8(a0 + 0x138u);
    table = RD32(RD32(a0 + 0x12Cu) + 24u);
    sp = m->sp_get(m->ctx);
    m->sp_set(m->ctx, sp - 24u);
    result = m->call4(m->ctx, 0x800BB490u, a0, RD8(table + 48u * index + 2u), a0, a3);
    m->sp_set(m->ctx, sp);
    return result;
}
