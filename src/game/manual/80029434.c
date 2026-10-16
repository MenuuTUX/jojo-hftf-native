#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_80011334(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);

/* 80029434 (TEXT). Resets the state block at 0x80066C48 (byte/half stores, exactly as the MIPS),
 * stores 451 at +0x1A, then calls 80025258 and 80024E74 and tail-calls man_80011334(3, 1, a2, a3).
 * $a1, $a2, $a3 are passed unchanged to the first call of 80010998 (nargs 4).
 * $a0 is set to 1 and 2 before the two calls to 80010998; the calls after that get $a0 = 2 as the
 * original code leaves it (the value the callees leave in $a0 is not modelled).
 * 80024E74 writes $a0..$a3 before reading them, and on return leaves $a0 = 0x180, $a1 = 0, $a2 = 3,
 * $a3 = 0x8006D308 (its loop constants). So the final call gets those values, not the entry $a2/$a3. */
uint32_t man_80029434(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t b = 0x80066C48u;

    (void)a0;
    m->call4(m->ctx, 0x80010998u, 1u, a1, a2, a3);
    m->call4(m->ctx, 0x80010998u, 2u, a1, a2, a3);

    WR8(b + 0x27u, 0u);
    WR8(b + 0x2Cu, 0u);
    WR16(b + 0x16u, 0u);
    WR16(b + 0x14u, 0u);
    WR16(b + 0x12u, 0u);
    WR16(b + 0x10u, 0u);
    WR16(b + 0x0Eu, 0u);
    WR16(b + 0x0Cu, 0u);
    WR16(b + 0x0Au, 0u);
    WR16(b + 0x08u, 0u);
    WR16(b + 0x06u, 0u);
    WR16(b + 0x04u, 0u);
    WR16(b + 0x02u, 0u);
    WR16(b + 0x00u, 0u);
    WR16(b + 0x1Au, 451u);
    WR8(b + 0x1Eu, 0u);
    WR8(b + 0x1Fu, 0u);

    m->call4(m->ctx, 0x80025258u, 2u, a1, a2, a3);
    m->call4(m->ctx, 0x80024E74u, 2u, a1, a2, a3);

    return man_80011334(m, 3u, 1u, 3u, 0x8006D308u);
}
