#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_8002816C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
uint32_t man_8002CAB0(const GuestMem *m);

/* 800294C4 (TEXT). $a0 is a state block: byte +31 is the active flag, +30 and +34 are flags.
 * Entry: if +31 is set, call 8002816C(a0..a3), 8002CB7C, then return the $v0 of 8002CBE4.
 * Otherwise return 0x8006CED4 when it is set. Else pick the flag (+31 = 1 or 2) from the pads
 * at 0x8006CEB8/0x8006CEC8; if none, return 0. Else run the reset calls, 80023AF4 (5 args, the fifth
 * in the caller frame slot), 8002438C(136), 8001729C(6, 10) when 0x8006D301 is 0, and return -128 unless
 * 0x8006CEE0 is -128. Return the sign-extended byte 0x8006D2DA when nonzero. Else fill the table at
 * 0x8006D1E8 and call 80027C28; the return is stored in 0x8006CE20 and 0x80070000 is returned when it
 * is null. Otherwise store the 64 / byte quotient and set bit 0 of the byte at 0x80073CD3; return that byte | 1.
 * The callees after the first call of a path get 0 arguments: the original leaves them to the callee. */
uint32_t man_800294C4(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t r, d, q, v4;

    if (RD8(a0 + 31u) != 0) {
        man_8002816C(m, a0, a1, a2, a3);
        m->call4(m->ctx, 0x8002CB7Cu, 0, 0, 0, 0);
        return m->call4(m->ctx, 0x8002CBE4u, 0, 0, 0, 0);
    }

    r = RD32(0x8006CED4u);
    if (r != 0)
        return r;

    if ((RD8(a0 + 34u) & 1u) != 0 && ((RD16(0x8006CEBEu) & 0x800u) != 0 || RD8(0x8006CEB8u) == 0xFFu))
        WR8(a0 + 31u, 1u);
    else if ((RD8(a0 + 34u) & 2u) != 0 && ((RD16(0x8006CECEu) & 0x800u) != 0 || RD8(0x8006CEC8u) == 0xFFu))
        WR8(a0 + 31u, 2u);

    if (RD8(a0 + 31u) == 0)
        return 0u;

    m->call4(m->ctx, 0x80025258u, a0, a1, a2, a3);
    man_8002CAB0(m);
    m->call4(m->ctx, 0x8002D138u, a0, a1, a2, a3);

    WR8(a0 + 30u, 0u);
    m->call5(m->ctx, 0x80023AF4u, 128u, 0u, 0u, 0u, 0u);
    m->call4(m->ctx, 0x8002438Cu, 136u, 0u, 0u, 0u);

    if (RD8(0x8006D301u) == 0)
        m->call4(m->ctx, 0x8001729Cu, 6u, 10u, 0u, 0u);

    if ((int32_t)RDS8(0x8006CEE0u) != -128)
        return 0xFFFFFF80u;

    if (RDS8(0x8006D2DAu) != 0)
        return (uint32_t)(int32_t)RDS8(0x8006D2DAu);

    WR16(0x8006D1E8u, 0u);
    WR16(0x8006D1EAu, 480u);
    WR16(0x8006D1ECu, 384u);
    WR16(0x8006D1EEu, 30u);
    v4 = m->call4(m->ctx, 0x80027C28u, 0u, 1u, 0x8006D1E8u, 8u);
    WR32(0x8006CE20u, v4);
    if (v4 == 0)
        return 0x80070000u;

    d = RD8(v4 + 7u);
    q = d ? (uint32_t)((int32_t)64 / (int32_t)d) : 0u;
    WR16(v4 + 8u, 64u);
    WR8(v4 + 6u, q);

    r = RD8(0x80073CD3u) | 1u;
    WR8(0x80073CD3u, r);
    return r;
}
