#include "manual.h"
#include "hex2c_rt.h"

/* 8001569C: clears the byte and word fields of the block at 0x80077E98 (0x80077E98 + offsets listed below) and returns
 * 0x80077E98. Every store is a single byte (sb) except the words at +0x08 and +0x0C. Bytes +0x1C and +0x1E are not written. */
uint32_t man_8001569C(const GuestMem *m) {
    const uint32_t base = 0x80077E98u;
    WR8(base + 0x01u, 0);
    WR8(base + 0x00u, 0);
    WR8(base + 0x07u, 0);
    WR8(base + 0x06u, 0);
    WR8(base + 0x05u, 0);
    WR8(base + 0x04u, 0);
    WR8(base + 0x03u, 0);
    WR8(base + 0x02u, 0);
    WR8(base + 0x1Du, 0);
    WR8(base + 0x1Bu, 0);
    WR8(base + 0x1Au, 0);
    WR8(base + 0x19u, 0);
    WR8(base + 0x18u, 0);
    WR8(base + 0x17u, 0);
    WR8(base + 0x16u, 0);
    WR32(base + 0x0Cu, 0);
    WR32(base + 0x08u, 0);
    WR8(base + 0x15u, 0);
    WR8(base + 0x14u, 0);
    WR8(base + 0x20u, 0);
    WR8(base + 0x1Fu, 0);
    return base;
}
