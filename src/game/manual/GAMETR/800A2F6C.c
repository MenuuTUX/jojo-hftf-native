#include "manual.h"
#include "hex2c_rt.h"

/* GAMETR 800A2F6C..800A3034. Hex-Rays treated the mutable queue count
 * at 800A3774 as constant, dropping the capacity branch and record indexing. */
uint32_t man_GAMETR_800A2F6C(const GuestMem *m, uint32_t a0, uint32_t a1) {
    const uint32_t state = 0x800A3738u, records = 0x800A30F8u;
    uint32_t result;
    if (RD8(state + 0x3Cu) == 40u) return 40u;
    WR16(state + 0x24u, RD16(a0));
    WR16(state + 0x26u, 224u - RD16(a0 + 2u));
    WR8(records + 8u * RD8(state + 0x3Cu) + 3u, RD8(a0 + 4u));
    WR16(records + 8u * RD8(state + 0x3Cu) + 4u, RD16(state + 0x24u));
    WR16(records + 8u * RD8(state + 0x3Cu) + 6u, RD16(state + 0x26u));
    WR8(records + 8u * RD8(state + 0x3Cu) + 2u, 0);
    WR16(records + 8u * RD8(state + 0x3Cu),
         RD16(0x800540E0u + 2u * ((RD8(a1) - 33u) & 0xFFFFu)));
    result = RD8(state + 0x3Cu) + 1u;
    WR8(state + 0x3Cu, result);
    return result;
}
