#include "manual.h"
#include "hex2c_rt.h"

/* 8002090C: fills the display-buffer fields of the object a0 from the index word 0x80066E14 and the halfword
 * table at 0x80066CD8/0x80066D70. No calls. Returns the halfword at 0x80066DEA. */
uint32_t man_8002090C(const GuestMem *m, uint32_t a0) {
    uint32_t v = RD32(0x80066E14u);

    WR16(a0 + 0x8Cu, 0);
    WR16(a0 + 0x54u, 0);
    WR16(a0 + 0x4Cu, 0);
    WR32(a0 + 4u, 0x800797C8u + 15360u * v);
    WR32(a0 + 0x10u, 0x80080FC8u + 1536u * v);
    WR16(a0 + 0x4Eu, RD16(0x80066D4Eu));
    WR16(a0 + 0x56u, RD16(0x80066D52u));
    WR16(a0 + 0x50u, RD16(0x80066DE6u));
    WR16(a0 + 0x58u, RD16(0x80066DEAu));
    WR16(a0 + 0x52u, RD16(0x80066DE6u));
    WR16(a0 + 0x5Au, RD16(0x80066DEAu));
    return RD16(0x80066DEAu);
}
