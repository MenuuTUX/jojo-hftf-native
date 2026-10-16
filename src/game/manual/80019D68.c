#include "manual.h"
#include "hex2c_rt.h"

/* 80019D68: resets the 0x80089F50 table and the 0x800797B0 rect, calls 8001ED60(0x29), LoadImage, then 80027414(2, rect,
 * data). Returns the $v0 of 80027414. */
uint32_t man_80019D68(const GuestMem *m) {
    WR32(0x80089FC0u, 0);
    WR32(0x80089F80u, 0x801F1800u);
    WR8(0x80089F58u, 60);
    WR8(0x80089F59u, 0);
    m->call4(m->ctx, 0x8001ED60u, 0x29u, 0u, 0u, 0u);
    WR16(0x800797B0u, 0);
    WR16(0x800797B2u, 504);
    WR16(0x800797B4u, 384);
    WR16(0x800797B6u, 2);
    m->call4(m->ctx, 0x80036348u, 0x800797B0u, 0x8010D800u, 0u, 0u);
    return m->call4(m->ctx, 0x80027414u, 2u, 0x800797B0u, 0x8010D800u, 0u);
}
