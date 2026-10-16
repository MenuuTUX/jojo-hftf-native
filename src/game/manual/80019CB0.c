#include "manual.h"
#include "hex2c_rt.h"

/* 80019CB0 (TEXT). $a0 = index (byte). Returns the address 0x80089F50 (in $v0 at jr ra).
 * Calls 8001ED60 with the halfword at 0x80059EC0[2*index] and zeros in $a1..$a3.
 * Then fills the 4-halfword RECT at 0x800797B0 and calls 80036348 (LoadImage-like) and 80027414. */
uint32_t man_80019CB0(const GuestMem *m, uint32_t a0) {
    uint32_t rect = 0x800797B0u;
    uint32_t v;

    v = RD16(0x80059EC0u + 2u * (a0 & 0xFFu));
    m->call4(m->ctx, 0x8001ED60u, v, 0u, 0u, 0u);

    WR16(rect + 0u, 0u);
    WR16(rect + 2u, 506u);
    WR16(rect + 4u, 384u);
    WR16(rect + 6u, 2u);

    m->call4(m->ctx, 0x80036348u, rect, 0x8010D800u, 0u, 0u);
    m->call4(m->ctx, 0x80027414u, 2u, rect, 0x8010D800u, 0u);

    WR32(0x80089FD4u, 0u);
    WR32(0x80089F94u, 0x801F9700u);
    WR8(0x80089F62u, 56u);
    WR8(0x80089F63u, 0u);

    return 0x80089F50u;
}
