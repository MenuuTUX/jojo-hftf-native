#include "manual.h"
#include "hex2c_rt.h"

/* 80019EB4: same as 80019E0C, with 8001ED60(0x209) as the first call. Returns the $v0 of 80027414. */
uint32_t man_80019EB4(const GuestMem *m) {
    WR32(0x80089FC4u, 0);
    WR32(0x80089F84u, 0x801F1E00u);
    WR8(0x80089F5Au, 56);
    WR8(0x80089F5Bu, 16);
    m->call4(m->ctx, 0x8001ED60u, 0x209u, 0u, 0u, 0u);
    WR16(0x800797B0u, 0);
    WR16(0x800797B2u, 491);
    WR16(0x800797B4u, 384);
    WR16(0x800797B6u, 3);
    m->call4(m->ctx, 0x80036348u, 0x800797B0u, 0x8010D800u, 0u, 0u);
    return m->call4(m->ctx, 0x80027414u, 2u, 0x800797B0u, 0x8010D800u, 0u);
}
