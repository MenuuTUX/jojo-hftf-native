#include "manual.h"
#include "hex2c_rt.h"

/* 800281B0 (TEXT). No arguments. p = *(0x8005A930) (a pointer). Increments the byte at p, clears p+1..p+3 and p+8,
 * sets p+12 to 1, then sets 0x8008EBC0 from 0x80066C6C and *(0x8005A92C). Tail-calls 8002DC84 when the mode is 2,
 * else 8002DBC4, and returns its $v0. $a0 = 0x80060000 at both calls (left by the lui at entry). */
uint32_t man_800281B0(const GuestMem *m) {
    uint32_t p = RD32(0x8005A930u);
    uint32_t mode;
    int32_t v1;

    WR8(p, RD8(p) + 1u);                    /* ++*off_8005A930 */
    WR8(p + 3u, 0u);
    WR8(p + 2u, 0u);
    WR8(p + 1u, 0u);
    WR8(p + 8u, 0u);
    WR8(p + 12u, 1u);
    WR8(0x8008EBC1u, 0u);
    WR8(0x8008EBC2u, 0u);

    v1 = (int32_t)RDS8(0x80066C6Cu);
    if ((v1 == 0 && (int32_t)RDS8(RD32(0x8005A92Cu)) != 0) || v1 == 5) {
        WR8(0x8008EBC0u, 1u);
    } else {
        mode = (v1 == 3) ? 2u : 0u;
        WR8(0x8008EBC0u, mode);
        WR8(0x800959E8u, 0u);
        WR8(0x800959E9u, 0u);
        WR8(0x800959EAu, 0u);
        WR8(0x800959EBu, 0u);
        WR8(0x800959ECu, 0u);
        WR8(0x800959EDu, 0u);
    }

    if (RD8(0x8008EBC0u) == 2u) {
        return m->call4(m->ctx, 0x8002DC84u, 0x80060000u, 0u, 0u, 0u);
    }
    return m->call4(m->ctx, 0x8002DBC4u, 0x80060000u, 0u, 0u, 0u);
}
