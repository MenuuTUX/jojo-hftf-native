#include "manual.h"
#include "hex2c_rt.h"

/* 80010DD4: scales the three colour bytes at 0x80066E1A..1C by the byte at 0x8006D1B0 (product >> 7) into the
 * struct that 0x80066E10 points to (+0x42D, +0x42E, +0x42F). Then LoadImage for each entry of the list at 0x80073CC4
 * (count = halfword 0x8006D48C, entries 16 bytes, source = word at entry+8), clears the count, calls 0x8002738C with $a0
 * (0x80070000 when the count was not positive, otherwise the entry pointer after the loop), and returns the value of
 * 0x8001121C. The $a1..$a3 handed to 0x8002738C and 0x8001121C are not modelled (passed as 0). */
uint32_t man_80010DD4(const GuestMem *m) {
    uint32_t d, x, y, z, p, s0, a0_out, result;
    int32_t s1;

    d = RD8(0x8006D1B0u);
    x = RD8(0x80066E1Au) * d;
    y = RD8(0x80066E1Bu) * d;
    z = RD8(0x80066E1Cu) * d;
    p = RD32(0x80066E10u);
    WR8(p + 0x42Du, x >> 7);
    WR8(p + 0x42Eu, y >> 7);
    WR8(p + 0x42Fu, z >> 7);

    s0 = RD32(0x80073CC4u);
    if (RDS16(0x8006D48Cu) > 0) {
        s1 = 0;
        do {
            m->call4(m->ctx, 0x80036348u, s0, RD32(s0 + 8u), 0, 0);
            s0 += 16u;
            s1++;
        } while (s1 < RDS16(0x8006D48Cu));
        a0_out = s0;
    } else {
        a0_out = 0x80070000u;
    }

    WR16(0x8006D48Cu, 0);
    m->call4(m->ctx, 0x8002738Cu, a0_out, 0, 0, 0);
    result = m->call4(m->ctx, 0x8001121Cu, 0, 0, 0, 0);
    return result;
}
