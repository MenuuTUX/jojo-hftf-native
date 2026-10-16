#include "manual.h"
#include "hex2c_rt.h"

/* 800F4960 (PL02X). $a0 = A (struct pointer). Returns 0x800FF33C (in $v0 at jr ra).
 * Copies the 29 words at 0x800FF364 to the buffer at A+0x12C, then fills the struct at A+0x130
 * (halfword at +0x1C, bytes at +0x24..+0x35, four code/data pointers) and stores the 0x800FF33C
 * pointer to A+0xBC. Copies A+0x1E (halfword) to A+0x84 and A+0xB0 (byte) to A+0xBA. */
uint32_t man_800F4960(const GuestMem *m, uint32_t a0) {
    uint32_t a = a0;
    uint32_t p = RD32(a + 0x12Cu);
    uint32_t q;
    uint32_t i;
    uint32_t v3;
    uint32_t v4;

    for (i = 0; i < 29u; i++)
        WR32(p + 4u * i, RD32(0x800FF364u + 4u * i));

    q = RD32(a + 0x130u);
    WR16(q + 0x1Cu, 0xFFECu);
    WR8(q + 0x25u, 1u);
    WR8(q + 0x26u, 1u);
    WR8(q + 0x33u, 1u);
    WR32(q + 0x00u, 0x801078FCu);
    WR32(q + 0x04u, 0x8010792Cu);
    WR32(q + 0x08u, 0x8010826Cu);
    WR32(q + 0x0Cu, 0x800FF320u);
    WR32(q + 0x10u, 0x800FF344u);
    WR32(q + 0x14u, 0x800FF350u);
    WR8(q + 0x24u, 0u);
    WR8(q + 0x28u, 0u);
    WR8(q + 0x29u, 0u);
    WR8(q + 0x34u, 0u);
    WR8(q + 0x35u, 0u);

    v3 = RD16(a + 0x1Eu);
    v4 = RD8(a + 0xB0u);
    WR32(a + 0xBCu, 0x800FF33Cu);
    WR16(a + 0x84u, v3);
    WR8(a + 0xBAu, v4);

    return 0x800FF33Cu;
}
