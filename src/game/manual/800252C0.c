#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800252C0(const GuestMem *m) {
    uint32_t i;
    uint32_t e = 0x80065EF0u;   /* a1 in the MIPS: entry table pointer + 8 */
    uint32_t p = 0x8010D800u;   /* a2 in the MIPS: 1 KB stride buffer pointer */

    WR8(0x800636E4u, 0u);
    WR16(0x8006D48Au, 32u);
    WR32(0x80073CC4u, 0x80065EE8u);

    /* 80025300..80025318: 32 entries of 16 bytes, a2 advances 1024 per entry */
    for (i = 0u; i < 32u; i++) {
        WR16(e - 4u, 4u);
        WR16(e - 2u, 128u);
        WR32(e, p);
        e += 16u;
        p += 1024u;
    }
    /* 80025328: sh zero in the jr delay slot; v0 = 0x80070000 at jr ra */
    WR16(0x8006D48Cu, 0u);
    return 0x80070000u;
}
