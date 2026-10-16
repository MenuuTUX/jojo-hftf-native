#include "manual.h"
#include "hex2c_rt.h"

/* 800A2070 (GAMESS): copies two per-player byte tables into the struct at *0x800A34E8 and the table at 0x8006D2D0, then
 * returns the byte at P[4] + 0x800A35D4 in $v0 (the last lbu before jr ra; the sb in its delay slot stores it to 0x8006D2D1).
 * P is re-read from 0x800A34E8 before each use, as the original reloads it. s = (int8)byte_80066C6A >> 1 (sll 24 / sra 25). */
uint32_t man_800A2070(const GuestMem *m) {
    uint32_t p, s, s1, v0;

    p = RD32(0x800A34E8u);
    WR8(p, RD8(RD8(p + 4u) + 0x800A355Cu));

    p = RD32(0x800A34E8u);
    WR8(p + 1u, RD8(RD8(p + 4u) + 0x800A3584u));

    s = (uint32_t)((int32_t)RDS8(0x80066C6Au) >> 1);
    p = RD32(0x800A34E8u);
    WR8(0x8006D2D4u + s, RD8(RD8(p + 4u) + 0x800A3584u));

    s1 = s ^ 1u;
    p = RD32(0x800A34E8u);
    v0 = RD8(RD8(p + 4u) + 0x800A35ACu);
    WR8(0x8006D2D4u + s1, v0);

    p = RD32(0x800A34E8u);
    v0 = RD8(RD8(p + 4u) + 0x800A35D4u);
    WR8(0x8006D2D1u, v0);
    return v0;
}
