#include "manual.h"
#include "hex2c_rt.h"

/* 80025BBC (TEXT). Reads no argument register (nargs 0). idx = signed half at 0x8006D48C, lim = signed half at
 * 0x8006D48A. If idx == lim, returns 0. Otherwise stores (low 16 bits of the unsigned half at 0x8006D48C) + 1 back to
 * 0x8006D48C, and returns the word at 0x80073CC4 + 16 * idx. */
uint32_t man_80025BBC(const GuestMem *m) {
    int32_t idx = RDS16(0x8006D48Cu);
    int32_t lim = RDS16(0x8006D48Au);
    uint32_t next = (uint32_t)RD16(0x8006D48Cu) + 1u;

    if (idx == lim)
        return 0u;
    WR16(0x8006D48Cu, next);
    return RD32(0x80073CC4u) + ((uint32_t)idx << 4);
}
