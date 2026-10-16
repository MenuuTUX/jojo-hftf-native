#include "manual.h"
#include "hex2c_rt.h"

/* Original: 800250F8 (TEXT, 156 bytes). Clears a 0x2E-byte table at base, where base is the
   constant 0x8006CE30 when $a0 (masked to 8 bits) is 3, else the constant address 0x80078244
   (the MIPS does "addiu a0,v0,-32188", so the address itself is the base, not the word at it). */
uint32_t man_800250F8(const GuestMem *m, uint32_t a0) {
    uint32_t base;
    uint32_t i;

    if ((a0 & 0xFFu) == 3u) {
        base = 0x8006CE30u;
    } else {
        base = 0x80078244u;
    }

    for (i = 0; i < 8u; i++) {
        WR16(base + 2u * i, i);
        WR16(base + 16u + 2u * i, i);
    }

    WR8(base + 0x21u, 2);
    WR8(base + 0x27u, 3);
    WR8(base + 0x28u, 100);
    WR8(base + 0x20u, 1);
    WR8(base + 0x22u, 1);
    WR8(base + 0x23u, 1);
    WR8(base + 0x24u, 0);
    WR8(base + 0x25u, 1);
    WR8(base + 0x26u, 0);
    WR8(base + 0x29u, 127);
    WR16(base + 0x34u, 0);
    WR16(base + 0x32u, 0);
    WR16(base + 0x30u, 0);
    WR16(base + 0x2Eu, 0);
    WR8(base + 0x2Au, 0);

    return 127u;
}
