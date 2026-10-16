#include "manual.h"
#include "hex2c_rt.h"

/* 80031490 (TEXT). $a0 = object (S1), $a1 = state (S0). Returns 1 when the counter at S0+5 is below 5 (no other effect
 * then), else 2. $a3 is not modelled: none of the callees 80031658, 800317B4, 8002438C, 80031A78 reads $a3 before writing
 * it (checked), so it is passed as 0. $a2 is set explicitly for 80031658 and 800317B4; 8002438C and 80031A78 do not read
 * $a2 before writing it (checked), so 0 is passed there. */
uint32_t man_80031490(const GuestMem *m, uint32_t a0, uint32_t a1) {
    uint32_t s1 = a0;
    uint32_t s0 = a1;
    uint8_t b;
    int32_t v3;
    int32_t v6;
    int32_t v7;
    uint32_t v1;

    b = (uint8_t)(RD8(s0 + 5u) + 1u);
    WR8(s0 + 5u, b);
    v3 = (int8_t)b;
    if (v3 < 5)
        return 1u;

    WR8(s0 + 5u, 5u);

    v1 = RD16(0x8006CEBEu) | RD16(0x8006CEBAu);
    if ((v1 & 0x5000u) != 0u) {
        m->call4(m->ctx, 0x80031658u, s1, s0, 0u, 0u);
    } else if ((v1 & 0xA0A0u) != 0u) {
        m->call4(m->ctx, 0x800317B4u, s1, s0, 0u, 0u);
    }

    v1 = RD16(0x8006CECEu) | RD16(0x8006CECAu);
    if ((v1 & 0x5000u) != 0u) {
        m->call4(m->ctx, 0x80031658u, s1, s0, 1u, 0u);
    } else if ((v1 & 0xA0A0u) != 0u) {
        m->call4(m->ctx, 0x800317B4u, s1, s0, 1u, 0u);
    }

    if ((RD16(0x8006CEBEu) & 0x840u) != 0u) {
        v6 = RDS8(s0 + 3u);
        if (v6 == 9) {
            m->call4(m->ctx, 0x8002438Cu, 138u, s0, 0u, 0u);
            m->call4(m->ctx, 0x80031A78u, s1, 0u, 0u, 0u);
        } else if (v6 == 10) {
            m->call4(m->ctx, 0x8002438Cu, 138u, s0, 0u, 0u);
            WR8(s0 + 1u, 2u);
        }
    }

    if ((RD16(0x8006CECEu) & 0x840u) != 0u) {
        v7 = RDS8(s0 + 4u);
        if (v7 == 9) {
            m->call4(m->ctx, 0x8002438Cu, 138u, s0, 0u, 0u);
            m->call4(m->ctx, 0x80031A78u, s1, 1u, 0u, 0u);
        } else if (v7 == 10) {
            m->call4(m->ctx, 0x8002438Cu, 138u, s0, 0u, 0u);
            WR8(s0 + 1u, 2u);
        }
    }

    if (((RD16(0x8006CEBEu) | RD16(0x8006CECEu)) & 0x10u) != 0u)
        WR8(s0 + 1u, 2u);
    return 2u;
}
