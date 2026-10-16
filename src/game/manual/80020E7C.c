#include "manual.h"
#include "hex2c_rt.h"

/* 80020E7C: scan the 12-byte entries at *0x80089F48, count 0x80081BC8.
 * Return and clear the first occupied entry with state byte +2 == 0;
 * otherwise release and return the occupied entry with the lowest nonzero
 * state byte. */
uint32_t man_80020E7C(const GuestMem *m) {
    uint32_t table = RD32(0x80089F48u);
    uint32_t count = RD16(0x80081BC8u);
    uint32_t selected = 0u;
    uint32_t lowest = 0xFFu;

    for (uint32_t i = 0u; i < count; ++i) {
        uint32_t entry = table + i * 12u;
        if (RD8(entry + 3u) == 0u) {
            continue;
        }

        uint32_t state = RD8(entry + 2u);
        if (state == 0u) {
            WR8(entry + 3u, 0u);
            WR32(entry + 4u, 0u);
            return entry;
        }
        if (state < lowest) {
            lowest = state;
            selected = entry;
        }
    }

    if (selected != 0u) {
        WR8(selected + 3u, 0u);
        WR32(selected + 4u, 0u);
    }
    return selected;
}
