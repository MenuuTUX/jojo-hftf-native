#include "manual.h"
#include "hex2c_rt.h"

/* 800179A8: disc-seek step on the object at $a0. Unless the byte at +0x31 is set, and 0x80049EDC returns 1: set +0x2D = 6,
 * DsIntToPos(+0x14 position, +0x08), wait for GsGetWorkBase to clear (calling 0x80049E4C each time), set the ready and sync
 * callbacks, then wait on the 0x800495E0 command set (9, 14 with +0x2B, 13 with +0x04, 21 with +0x08, 27). Always clears +0x3C
 * and returns the last $v0. $a1..$a3 of the plain calls are not modelled (passed as 0). */
uint32_t man_800179A8(const GuestMem *m, uint32_t a0) {
    uint32_t v0;

    v0 = RD8(a0 + 0x31u);
    if (v0 == 0u) {
        v0 = m->call4(m->ctx, 0x80049EDCu, a0, 0, 0, 0);
        if (v0 == 1u) {
            WR8(a0 + 0x2Du, 6u);
            m->call4(m->ctx, 0x8004CB80u, RD32(a0 + 0x14u), a0 + 8u, 0, 0);
            while (m->call4(m->ctx, 0x80049F28u, 0, 0, 0, 0) != 0u)
                m->call4(m->ctx, 0x80049E4Cu, 0, 0, 0, 0);
            m->call4(m->ctx, 0x8004D834u, 0, 0, 0, 0);
            m->call4(m->ctx, 0x8004D820u, 0x80017CC8u, 0, 0, 0);
            do {
                v0 = m->call4(m->ctx, 0x800495E0u, 9, 0, 0, 0xFFFFFFFFu);
            } while (v0 == 0u);
            do {
                v0 = m->call4(m->ctx, 0x800495E0u, 14, a0 + 0x2Bu, 0, 0xFFFFFFFFu);
            } while (v0 == 0u);
            do {
                v0 = m->call4(m->ctx, 0x800495E0u, 13, a0 + 4u, 0, 0xFFFFFFFFu);
            } while (v0 == 0u);
            do {
                v0 = m->call4(m->ctx, 0x800495E0u, 21, a0 + 8u, 0, 0xFFFFFFFFu);
            } while (v0 == 0u);
            do {
                v0 = m->call4(m->ctx, 0x800495E0u, 27, 0, 0, 0xFFFFFFFFu);
            } while (v0 == 0u);
        }
    }
    WR8(a0 + 0x3Cu, 0);
    return v0;
}
