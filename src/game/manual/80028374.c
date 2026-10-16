#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800291F4(const GuestMem *m, uint32_t a0);

/* 80028374 (TEXT). Copies 10 bytes from 0x8006CEB8 (bit 0 of the byte at 0x80066C67 set) or 0x8006CEC8 into its own frame,
 * as the words a2, a3 and the halfwords it reads back (v15 = base+2, v16 = base+6). The frame is not read by any callee
 * and not read after a call, so it is not reproduced in guest memory. $a0 is overwritten with 0 before any read; $a1 is
 * passed on to 8002438C. $a2/$a3 are the copied words. */
uint32_t man_80028374(const GuestMem *m, uint32_t a0, uint32_t a1)
{
    uint32_t base, a2, a3, v15, v16, ptr, b, v2, v1, v0;

    base = (RD8(0x80066C67u) & 1u) ? 0x8006CEB8u : 0x8006CEC8u;
    a2 = RD32(base);
    a3 = RD32(base + 4u);
    v15 = RD16(base + 2u);
    v16 = RD16(base + 6u);

    if ((v16 & 0x800u) != 0u) {
        man_800291F4(m, 0u);
        return m->call4(m->ctx, 0x8002438Cu, 138u, a1, a2, a3);
    }

    if ((v16 & 0x40u) != 0u) {
        m->call4(m->ctx, 0x8002438Cu, 138u, a1, a2, a3);
        a1 = 0x80060000u;
        ptr = RD32(0x8005A930u);
        if (RD8(ptr + 8u) == 0u)
            return man_800291F4(m, ptr);

        WR8(0x8008EBC1u, RD8(ptr + 8u) + 1u);
        WR8(ptr + 0x0Au, RD8(ptr + 8u));
        WR8(ptr + 1u, RD8(ptr + 8u));
        WR8(ptr + 0u, RD8(ptr + 0u) + 1u);
        WR8(ptr + 3u, 0u);
        WR8(ptr + 2u, 0u);

        b = RD8(0x8008EBC0u);
        if (b != 1u) {
            WR8(ptr + 8u, 1u);
            return ptr;
        }
        if (RD8(ptr + 8u) != 1u) {
            WR8(ptr + 8u, 1u);
            return 0x80060000u;
        }
        if ((RD8(0x80066C67u) & 1u) == 0u)
            v0 = (RD8(0x8006CE57u) >> 1) & 1u;
        else
            v0 = RD8(0x8006CE57u) & 1u;
        WR8(ptr + 8u, v0);
        return v0;
    }

    /* 0x40 clear: v2 is the $a0 value the original sets (0, 1 or 2) before it reaches the shared tail. */
    if ((v16 & 0x1000u) != 0u || (v16 & 0x4000u) != 0u) {
        v2 = ((v16 & 0x1000u) != 0u) ? 2u : 1u;
        ptr = RD32(0x8005A930u);
        WR8(ptr + 0x0Du, 32u);
    } else {
        a1 = 0x80060000u;
        ptr = RD32(0x8005A930u);
        if ((v15 & 0x1000u) != 0u) {
            WR8(ptr + 0x0Du, RD8(ptr + 0x0Du) - 1u);
            v2 = 0u;
            if (RDS8(ptr + 0x0Du) < 0) {
                WR8(ptr + 0x0Du, 8u);
                v2 = 2u;
            }
        } else if ((v15 & 0x4000u) != 0u) {
            WR8(ptr + 0x0Du, RD8(ptr + 0x0Du) - 1u);
            v2 = 0u;
            if (RDS8(ptr + 0x0Du) < 0) {
                WR8(ptr + 0x0Du, 8u);
                v2 = 1u;
            }
        } else {
            WR8(ptr + 0x0Du, 32u);
            v2 = 0u;
        }
    }

    if (v2 == 1u) {
        ptr = RD32(0x8005A930u);
        b = RD8(0x8008EBC0u);
        v1 = RD8(ptr + 8u);
        if (v1 != (uint32_t)(int32_t)RDS8(0x8005A934u + b))
            WR8(ptr + 8u, v1 + 1u);
        else
            WR8(ptr + 8u, 0u);
        m->call4(m->ctx, 0x8002438Cu, 137u, a1, a2, a3);
        return m->call4(m->ctx, 0x80028E08u, 0u, a1, a2, a3);
    }

    if (v2 == 2u) {
        ptr = RD32(0x8005A930u);
        if (RD8(ptr + 8u) != 0u) {
            WR8(ptr + 8u, RD8(ptr + 8u) - 1u);
        } else {
            b = RD8(0x8008EBC0u);
            WR8(ptr + 8u, RD8(0x8005A934u + b));
        }
        m->call4(m->ctx, 0x8002438Cu, 137u, a1, a2, a3);
    }
    return m->call4(m->ctx, 0x80028E08u, 0u, a1, a2, a3);
}
