#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_8001A418(const GuestMem *m) {
    uint32_t a0 = 0u;
    uint32_t v1;

    /* 8001A420..8001A45C: a0 = 2 if byte_8006D2D1 == 9, else 1 if byte_80066C6C is 0 or 5 (lb, signed), else 0 */
    if (RD8(0x8006D2D1u) == 9u) {
        a0 = 2u;
    } else {
        v1 = RDS8(0x80066C6Cu);
        if (v1 == 0u || v1 == 5u) {
            a0 = 1u;
        }
    }

    /* 8001A464..8001A47C: jal 8001ED60 with a0 = halfword table[a0], a1 = a2 = a3 = 0 */
    m->call4(m->ctx, 0x8001ED60u, RD16(0x80059EF4u + (a0 << 1)), 0u, 0u, 0u);

    /* 8001A484..8001A4B0: rect at 0x800797B0 = {0, 494, 384, 1}, LoadImage(rect, 0x8010D800) */
    WR16(0x800797B0u, 0u);
    WR16(0x800797B2u, 494u);
    WR16(0x800797B4u, 384u);
    WR16(0x800797B6u, 1u);
    m->call4(m->ctx, 0x80036348u, 0x800797B0u, 0x8010D800u, 0u, 0u);

    /* 8001A4B8..8001A4C8: jal 80027414 (mode 2, rect, data); a3 not set by the original here (GUESS: 0) */
    m->call4(m->ctx, 0x80027414u, 2u, 0x800797B0u, 0x8010D800u, 0u);

    /* 8001A4CC..8001A500 */
    WR32(0x80089FC8u, 0x801F2700u);
    WR8(0x80089F5Cu, 0u);
    WR8(0x80089F5Du, 0u);
    WR32(0x80089F88u, 0x801F8700u);

    return 0x80090000u;                     /* lui v0,0x8009 at 8001A4FC is the last write to $v0 */
}
