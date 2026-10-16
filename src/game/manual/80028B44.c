#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_80028E08(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
uint32_t man_8002917C(const GuestMem *m);

/* 80028B44 (TEXT), frame 0x28 (addiu sp,sp,-40). Copies a 10-byte record (word, word, halfword) from 0x8006CEB8 or 0x8006CEC8
 * (by bit 0 of 0x80066C67) to sp+0x10, sp+0x14, sp+0x18, sets bit 1 of 0x80073CD3, then:
 *  - if byte 0x8008EBC0 == 2: clears 0x8008EBC2 and calls 800A21BC with $a0..$a3 and the record as stack arguments 5..7;
 *  - else selects a path by the halfword at sp+0x16 (high half of the second word) and returns the $v0 of 80028E08.
 * $a1..$a3 are passed on unchanged where the original passes them on unchanged (entry values). After a call the original
 * passes on whatever the callee left in $a1..$a3; m->call4 does not return those, so the entry values are used here (see the
 * report: not measured whether the callees 8002438C and 80011160 change $a1..$a3). */
uint32_t man_80028B44(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx) - 0x28u;   /* 0x28 = the original "addiu sp,sp,-40" */
    uint32_t v12, v13, v14, w, p, v, b, ret;

    m->sp_set(m->ctx, sp);
    if ((RD8(0x80066C67u) & 1u) != 0u) {
        v12 = RD32(0x8006CEB8u);
        v13 = RD32(0x8006CEBCu);
        v14 = RD16(0x8006CEC0u);
    } else {
        v12 = RD32(0x8006CEC8u);
        v13 = RD32(0x8006CECCu);
        v14 = RD16(0x8006CED0u);
    }
    WR32(sp + 0x10u, v12);
    WR32(sp + 0x14u, v13);
    WR16(sp + 0x18u, v14);
    WR8(0x80073CD3u, RD8(0x80073CD3u) | 2u);

    if (RD8(0x8008EBC0u) == 2u) {
        WR8(0x8008EBC2u, 0u);
        ret = m->call4(m->ctx, 0x800A21BCu, a0, a1, a2, a3);
        goto done;
    }

    w = RD16(sp + 0x16u);
    if ((w & 0x840u) != 0u) {
        m->call4(m->ctx, 0x8002438Cu, 138u, a1, a2, a3);
        p = RD32(0x8005A930u);
        if (RD8(p + 8u) == 0u) {
            if ((RD8(0x80066C67u) & 1u) != 0u) {
                b = RD8(0x8006CE57u);
                WR8(0x8006CE57u, b & 2u);
                m->call4(m->ctx, 0x80011160u, 0u, 1u, 125u, 20u);
            } else {
                b = RD8(0x8006CE57u);
                WR8(0x8006CE57u, b & 1u);
                m->call4(m->ctx, 0x80011160u, 1u, 1u, 125u, 20u);
            }
        } else {
            if ((RD8(0x80066C67u) & 1u) != 0u) {
                b = RD8(0x8006CE57u);
                if ((b & 1u) == 0u) WR8(0x8006CE57u, b | 1u);
            } else {
                b = RD8(0x8006CE57u);
                if ((b & 2u) == 0u) WR8(0x8006CE57u, b | 2u);
            }
        }
        man_8002917C(m);
        ret = man_80028E08(m, 0u, a1, a2, a3);
        goto done;
    }

    if ((w & 0x1000u) != 0u) {
        p = RD32(0x8005A930u);
        v = RD8(p + 8u);
        if (v != 0u) {
            WR8(p + 8u, (v - 1u) & 0xFFu);
        } else {
            WR8(p + 8u, 1u);
        }
        m->call4(m->ctx, 0x8002438Cu, 137u, a1, a2, a3);
        ret = man_80028E08(m, 2u, a1, a2, a3);
        goto done;
    }

    if ((w & 0x4000u) == 0u) {
        ret = man_80028E08(m, 2u, a1, a2, a3);
        goto done;
    }

    p = RD32(0x8005A930u);
    v = RD8(p + 8u);
    if (v == 1u) {
        WR8(p + 8u, 0u);
    } else {
        WR8(p + 8u, (v + 1u) & 0xFFu);
    }
    m->call4(m->ctx, 0x8002438Cu, 137u, a1, a2, a3);
    ret = man_80028E08(m, 2u, a1, a2, a3);

done:
    m->sp_set(m->ctx, sp + 0x28u);
    return ret;
}
