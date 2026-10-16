#include "manual.h"
#include "hex2c_rt.h"

/* 800176E8 (TEXT). $a0 = S (struct pointer). Calls 80049EDC; if it returns 1, sets S fields, calls 8004CB80 with
 * (S.+0x0C, S+8), then waits in polling loops on 80049F28, 800495E0 and returns the last 800495E0 result.
 * Otherwise returns the 80049EDC result. The MIPS passes a0 of 80049F28/80049E4C as left by 8004CB80 (not set here). */
uint32_t man_800176E8(const GuestMem *m, uint32_t a0) {
    uint32_t s0 = a0;
    uint32_t v0 = m->call4(m->ctx, 0x80049EDCu, a0, 0u, 0u, 0u);
    uint32_t v3;

    if (v0 != 1u) {
        return v0;
    }
    v3 = RD32(s0 + 0x0Cu);
    WR8(s0 + 0x2Du, 2u);                    /* sb v0,0x2d(s0) */
    WR8(s0 + 0x29u, 0u);                    /* sb zero,0x29(s0) */
    WR8(s0 + 0x2Cu, 0u);                    /* sb zero,0x2c(s0) */
    WR8(s0 + 0x3Cu, 2u);                    /* sb v0,0x3c(s0) */
    WR32(s0 + 0x14u, v3);                   /* sw v1,0x14(s0): delay slot of jal 8004cb80 */
    m->call4(m->ctx, 0x8004CB80u, v3, s0 + 0x08u, 0u, 0u);
    WR16(s0 + 0x42u, 240u);                 /* sh v0,0x42(s0) */
    while (m->call4(m->ctx, 0x80049F28u, 0u, 0u, 0u, 0u) != 0u) {
        m->call4(m->ctx, 0x80049E4Cu, 0u, 0u, 0u, 0u);
    }
    m->call4(m->ctx, 0x8004D834u, 0u, 0u, 0u, 0u);
    m->call4(m->ctx, 0x8004D820u, 0x80017CC8u, 0u, 0u, 0u);
    do {
        v0 = m->call4(m->ctx, 0x800495E0u, 9u, 0u, 0u, 0xFFFFFFFFu);
    } while (v0 == 0u);
    do {
        v0 = m->call4(m->ctx, 0x800495E0u, 14u, s0 + 0x2Bu, 0u, 0xFFFFFFFFu);
    } while (v0 == 0u);
    do {
        v0 = m->call4(m->ctx, 0x800495E0u, 13u, s0 + 0x04u, 0u, 0xFFFFFFFFu);
    } while (v0 == 0u);
    do {
        v0 = m->call4(m->ctx, 0x800495E0u, 21u, s0 + 0x08u, 0u, 0xFFFFFFFFu);
    } while (v0 == 0u);
    do {
        v0 = m->call4(m->ctx, 0x800495E0u, 27u, 0u, 0u, 0xFFFFFFFFu);
    } while (v0 == 0u);
    return v0;
}
