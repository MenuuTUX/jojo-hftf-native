#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_80010F60(const GuestMem *m) {
    uint32_t pad1 = 0x8006CE68u;
    uint32_t pad2 = 0x8006CE90u;

    /* 80010F84 (delay slot of the first jal): byte 0x80055E84 = 0 */
    WR8(0x80055E84u, 0u);
    m->call4(m->ctx, 0x80035C50u, 0x80073CD8u, 32u, 0u, 0u);
    m->call4(m->ctx, 0x80035C50u, pad1, 34u, 0u, 0u);
    m->call4(m->ctx, 0x80035C50u, pad2, 34u, 0u, 0u);
    m->call4(m->ctx, 0x8004FD40u, pad1, pad2, 0u, 0u);
    /* 80010FBC: jal 8004D8A0, no arguments; its v0 is what the function leaves */
    return m->call4(m->ctx, 0x8004D8A0u, 0u, 0u, 0u, 0u);
}
