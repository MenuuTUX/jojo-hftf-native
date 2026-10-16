#include "manual.h"
#include "hex2c_rt.h"

/* Original: 80025194 (TEXT, 156 bytes). Copies 5 records of 12 bytes from the table at the
   constant address 0x8005A870 (source; 12-byte stride) into a destination that is the constant
   address 0x80073490 when $a0 (masked to 8 bits) is 3, else 0x8007827C. Each record: 4-byte word at
   offset 0, bytes at offsets 4..9. The MIPS keeps the addresses in registers, so they are used as
   addresses here, not dereferenced. Returns $v0 = (a3 < 5) after the loop, which is 0. */
uint32_t man_80025194(const GuestMem *m, uint32_t a0) {
    uint32_t src;
    uint32_t dst;
    uint32_t n;

    src = 0x8005A870u;
    if ((a0 & 0xFFu) == 3u) {
        dst = 0x80073490u;
    } else {
        dst = 0x8007827Cu;
    }

    for (n = 0; n < 5u; n++) {
        WR32(dst, RD32(src));
        WR8(dst + 4u, RD8(src + 4u));
        WR8(dst + 5u, RD8(src + 5u));
        WR8(dst + 6u, RD8(src + 6u));
        WR8(dst + 7u, RD8(src + 7u));
        WR8(dst + 8u, RD8(src + 8u));
        WR8(dst + 9u, RD8(src + 9u));
        src += 12u;
        dst += 12u;
    }

    return 0u;
}
