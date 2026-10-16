#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800291F4(const GuestMem *m, uint32_t a0) {
    uint32_t p, result;
    uint32_t quotient = 0;

    p = RD32(0x8005A930u);
    WR8(p + 0x0Cu, 0);
    WR8(p + 0x03u, 0);
    WR8(p + 0x02u, 0);
    WR8(p + 0x01u, 0);
    WR8(p + 0x00u, 0);

    WR8(0x80066C66u, 0);
    WR8(0x80066C67u, 0);
    /* a0 is passed on as received; 0x8002D138 overwrites $a0 before it reads it. */
    m->call4(m->ctx, 0x8002D138u, a0, 0, 0, 0);

    if (RDS8(0x8006D301u) == 0) {
        m->call4(m->ctx, 0x8001729Cu, 3, 0, 0, 0);
        m->call4(m->ctx, 0x8001729Cu, 5, 10, 0, 0);
    }

    if ((RD8(0x80073CD3u) & 1u) == 0u)
        return 0u;

    WR16(0x8006D1E8u, 0);
    WR16(0x8006D1EAu, 480);
    WR16(0x8006D1ECu, 384);
    WR16(0x8006D1EEu, 30);
    result = m->call4(m->ctx, 0x80027C28u, 0, 0, 0x8006D1E8u, 8);
    WR32(0x8006CE20u, result);

    /* MIPS 'div' with a zero divisor reaches the 'break 7' the compiler emits; that trap is not modelled here.
     * The divide is guarded so the C stays defined; quotient stays 0 in that case. */
    if (RD8(result + 7u) != 0u)
        quotient = 64u / RD8(result + 7u);
    WR16(result + 8u, 128);
    WR8(result + 6u, quotient);
    return result;
}
