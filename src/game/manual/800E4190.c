#include "manual.h"
#include "hex2c_rt.h"

/* 800E4190: frame of 24 bytes; state byte +7 selects one of six handlers through the jump table at 0x800DF0DC
 * (entries 800E41D0, 800E4264, 800E42E8, 800E43A4, 800E43DC, 800E43EC). State >= 6 returns 0x800E0000 (the delay-slot lui).
 * $a1 is loaded from +0x17C and stays unchanged until a call. Case 1 and 2 use table 0x800E7B80 (12-byte records).
 * The shared tail (800E43C4 / 800E43CC) calls 800BB648 (when needed) and returns the $v0 of 80026A04. */
uint32_t man_800E4190(const GuestMem *m, uint32_t a0) {
    uint32_t s0 = a0;
    uint32_t v5 = RD32(s0 + 0x17Cu);
    uint32_t st = RD8(s0 + 7u);
    uint32_t idx;
    uint32_t v9;
    uint32_t to13 = 0u;

    if (st >= 6u) {
        return 0x800E0000u;
    }

    switch (st) {
    case 0u:
        WR8(s0 + 7u, st + 1u);
        m->call4(m->ctx, 0x800E5CD0u, s0, v5, 0u, 0u);
        m->call4(m->ctx, 0x80024104u, s0, 279u, 0u, 0u);
        idx = RD8(s0 + 519u);
        WR16(s0 + 12u, RD8(0x800E7DD8u + idx));
        m->call4(m->ctx, 0x800BB2A4u, s0, RD8(0x800E7DD4u + idx), 0u, 0u);
        m->call4(m->ctx, 0x800BB30Cu, s0, 0u, 0u, 0u);
        idx = RD8(s0 + 519u);
        return m->call4(m->ctx, 0x800268BCu, s0,
                        RD32(0x800EBC38u + 4u * RD8(0x800E7DDCu + idx)), 0u, 0u);

    case 1u:
        if (RD8(s0 + 77u) != 0u) {
            idx = RD8(s0 + 519u);
            WR8(s0 + 7u, st + 1u);
            WR32(s0 + 112u, RD32(0x800E7B80u + 12u * idx));
            v9 = RD32(0x800E7B80u + 12u * idx + 4u);
            WR32(s0 + 124u, 0u);
            WR32(s0 + 116u, 0u);
            WR32(s0 + 120u, v9);
            if (RD8(s0 + 11u) != 0u) {
                WR32(s0 + 112u, 0u - RD32(s0 + 112u));
                WR32(s0 + 120u, 0u - RD32(s0 + 120u));
            }
        }
        break;

    case 2u:
        WR16(s0 + 12u, RD16(s0 + 12u) - 1u);
        if ((int16_t)RD16(s0 + 12u) < 0 || m->call4(m->ctx, 0x800E5CF4u, s0, v5, 0x50u, 0u) != 0u) {
            idx = RD8(s0 + 519u) + 3u;
            WR8(s0 + 7u, st + 1u);
            WR32(s0 + 112u, RD32(0x800E7B80u + 12u * idx));
            WR32(s0 + 120u, RD32(0x800E7B80u + 12u * idx + 4u));
            if (RD8(s0 + 11u) != 0u) {
                WR32(s0 + 112u, 0u - RD32(s0 + 112u));
                WR32(s0 + 120u, 0u - RD32(s0 + 120u));
            }
            m->call4(m->ctx, 0x800268BCu, s0, RD32(0x800EBC6Cu), 0u, 0u);
        }
        to13 = 1u;
        break;

    case 3u:
        if ((int8_t)RD8(s0 + 77u) == -1) {
            WR8(s0 + 77u, 0u);
            WR8(s0 + 7u, st + 1u);
        }
        to13 = 1u;
        break;

    case 4u:
        return m->call4(m->ctx, 0x800E5D1Cu, s0, v5, 0u, 0u);

    default: /* case 5 */
        return m->call4(m->ctx, 0x800E5D6Cu, s0, v5, 0u, 0u);
    }

    if (to13 != 0u) {
        m->call4(m->ctx, 0x800BB648u, s0, 0u, 0u, 0u);
    }
    return m->call4(m->ctx, 0x80026A04u, s0, 0u, 0u, 0u);
}
