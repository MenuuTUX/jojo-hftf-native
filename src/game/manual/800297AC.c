#include "manual.h"
#include "hex2c_rt.h"

/* 800297AC (TEXT). No register inputs. Returns 128 on every path ($v0 = 128 set in a delay slot).
 * The jump table at 0x80054DC0 (read from ram_fight1.bin) sends byte-9 indices 0, 8, 10, 11, 13 to the case body. */
uint32_t man_800297AC(const GuestMem *m) {
    uint32_t t;
    uint32_t a2v;
    uint32_t idx;
    uint32_t v5;

    m->call4(m->ctx, 0x80025280u, 0u, 0u, 0u, 0u);

    if (RD8(0x8006D2D1u) == 9u && RD16(0x8005AFECu) == 0u) {
        WR32(0x80066D60u, 0xFFA00000u);
        WR32(0x80066D64u, 0x00B00000u);
    } else {
        WR32(0x80066D60u, 0x00400000u);
        WR32(0x80066D64u, 0x013F0000u);
    }

    WR8(0x8008EBCFu, 0u);
    WR8(0x8008EBCEu, 0u);
    WR8(0x8008EBCDu, 0u);
    WR8(0x8008EBCCu, 0u);
    WR8(0x8008EBCBu, 0u);
    WR8(0x8008EBCAu, 0u);
    WR8(0x8008EBC9u, 0u);
    WR8(0x8008EBC8u, 0u);
    WR8(0x8008EBD0u, 0u);
    WR16(0x8005AFFAu, 0u);
    WR16(0x8005AFF8u, 0u);
    WR16(0x8005AFF6u, 0u);
    WR16(0x8005AFF4u, 0u);
    WR32(0x8005AFF0u, 0u);
    WR16(0x8005AFECu, 0u);
    WR16(0x8005AFEAu, 0u);
    WR16(0x8005AFE6u, 0u);
    WR16(0x8005AFE4u, 0u);
    WR16(0x8005AFE8u, 0u);
    WR16(0x8005AFE2u, 0u);

    t = RD8(RD8(0x8006D2D1u) + 0x8005AFB0u);
    a2v = t & 0xFu;
    v5 = ((t >> 4) == 3u) ? 0x00B00000u : 0x01400000u;

    WR32(0x80066CE8u, v5);
    WR32(0x80066D4Cu, v5);
    WR32(0x80066D20u, 0x8010B800u);
    WR8(0x80066CDFu, 48u);
    WR16(0x80066D1Cu, (t >> 4) << 4);
    WR16(0x80066D1Eu, a2v << 4);
    WR8(0x80066D28u, 6u);
    WR32(0x80066D50u, 0u);
    WR32(0x80066CECu, 0u);
    WR16(0x80066D24u, 432u);
    m->call4(m->ctx, 0x8002B4E4u, 0x80066CD8u, 0x8008EBD8u, a2v, 0u);

    idx = RD8(0x8006D2D1u) - 9u;
    if (idx == 0u || idx == 8u || idx == 10u || idx == 11u || idx == 13u) {
        WR32(0x80066DB8u, 0x8010C800u);
        WR8(0x80066D77u, 56u);
        WR16(0x80066DB4u, 32u);
        WR16(0x80066DB6u, 32u);
        WR8(0x80066DC0u, 6u);
        WR32(0x80066DE8u, 0u);
        WR32(0x80066D84u, 0u);
        WR16(0x80066DBCu, 432u);
        m->call4(m->ctx, 0x8002B4E4u, 0x80066D70u, 0x800921D8u, a2v, 0u);
    }

    WR32(0x80066D30u, 0x00020000u);
    WR32(0x80066D2Cu, 0u);
    if (RD8(0x8006D2D1u) == 32u) {
        WR8(0x80066D29u, 0x80u);
    }
    return 128u;
}
