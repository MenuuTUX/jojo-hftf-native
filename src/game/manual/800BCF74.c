#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800BCF74(const GuestMem *m, uint32_t a0) {
    uint32_t p = a0;
    uint32_t s, tbl, v0, a1v = 0, a2v = 0;
    int32_t sv;

    if (RD8(p + 8) == 0) {
        WR8(p + 8, 1);                      /* sb v0,0x8(s0) at 800bcf94 */
        m->call4(m->ctx, 0x800BCC04u, p, 61, 0, 0);   /* addiu a1,zero,61 is in the delay slot */
    }
    s = RD8(p + 8);                         /* lbu a0,0x8(s0) at 800bcfa0 */
    if (s == 1) {
        sv = RDS8(p + 0x145);
        if (sv > 0) {                       /* blez at 800bcfbc not taken */
            tbl = RD8(0x800CBA3Cu + (uint32_t)(int32_t)RDS16(p + 0xC));
            WR16(p + 0xC, (RD16(p + 0xC) + 1u) & 0xFu);
            WR8(p + 0x145, RD8(p + 0x145) - 1u);
            v0 = RD16(p + 0x6A) + (uint32_t)(int32_t)(int8_t)(uint8_t)tbl;
            WR16(p + 0x6A, v0);
            return v0;
        }
        /* blez taken: $v0 = state + 1 = 2 (delay slot), stored only when byte 0x169 is 0 */
        if (RD8(p + 0x169) == 0) {
            WR8(p + 8, 2);
        }
        return 2;
    }
    if (RD8(p + 0x169) != 0) {
        return 128;                         /* delay slot sets $v0 = 128 before the bne at 800bd030 */
    }
    if (RD8(p + 0x4D) == 128) {
        WR8(p + 0x4D, 0);
        m->call4(m->ctx, 0x80024564u, p, 0, 0, 0);
        m->call4(m->ctx, 0x800C7374u, p, 0, 0, 0);
        m->call4(m->ctx, 0x80029714u, p, 14, 8, 0);
        a1v = 14;
        a2v = 8;
    }
    if (s == 2) {
        uint32_t a1s = (uint32_t)(int32_t)RDS8(p + 0x148);   /* lb a1,0x148(s0) at 800bd070 */
        v0 = m->call4(m->ctx, 0x800BF1E0u, p, a1s, a2v, 0);
        if (v0 != 0) {
            v0 = RD8(p + 8) + 1u;           /* lbu v0,0x8(s0); addiu; sb v0,0x8(s0) in delay slot of j */
            WR8(p + 8, v0);
            return v0;
        }
        return m->call4(m->ctx, 0x80026A04u, p, a1s, a2v, 0);   /* 800bd0fc, $v0 entry = 0 (not passed) */
    }
    if (RDS8(p + 0x4D) == -1) {
        WR8(p + 7, 36);                     /* sb v0,0x7(s0) at 800bd0f0, $v0 = 36 */
        WR8(p + 8, 0);
        return 36;
    }
    return m->call4(m->ctx, 0x80026A04u, p, a1v, a2v, 0);   /* 800bd0fc, $v0 entry = 36 (not passed) */
}
