#include "manual.h"
#include "hex2c_rt.h"

/* 800BC698 (AC_MAIN). $a0 = A (struct, signed slot index byte at +0x136), $a1 = X.
 * Slot address v5 = 0x8006D2D0 + 4*slot + 0x28. Returns 1 at once when X == 1 and the byte at v5 is 0x99.
 * Otherwise: dword_800DAE54 = 800BC20C(X) (800BC20C reads only $a0; $a1 is still X at the call), then the four bytes
 * at v5 are replaced by 800BC19C(byte, byte of dword). If the flag byte 0x800DAE58 (set by 800BC19C) is 0, returns 0.
 * Else the word at v5 becomes (byte at v5) | 0x99999900 and that is returned. */
uint32_t man_800BC698(const GuestMem *m, uint32_t a0, uint32_t a1) {
    uint32_t s3 = a0;
    uint32_t s4 = 0x8006D2D0u;
    uint32_t v5;
    uint32_t r;
    uint32_t addr;

    if (RD8(s4 + 4u * (uint32_t)RDS8(s3 + 0x136u) + 0x28u) == 0x99u && a1 == 1u) {
        return 1u;
    }
    r = m->call4(m->ctx, 0x800BC20Cu, a1, a1, 0u, 0u);
    WR32(0x800DAE54u, r);
    WR8(0x800DAE58u, 0u);
    v5 = s4 + 4u * (uint32_t)RDS8(s3 + 0x136u) + 0x28u;
    r = m->call4(m->ctx, 0x800BC19Cu, RD8(v5), RD8(0x800DAE54u), 0u, 0u);
    WR8(v5, r);
    r = m->call4(m->ctx, 0x800BC19Cu, RD8(v5 + 1u), RD8(0x800DAE55u), 0u, 0u);
    WR8(v5 + 1u, r);
    r = m->call4(m->ctx, 0x800BC19Cu, RD8(v5 + 2u), RD8(0x800DAE56u), 0u, 0u);
    WR8(v5 + 2u, r);
    r = m->call4(m->ctx, 0x800BC19Cu, RD8(v5 + 3u), RD8(0x800DAE57u), 0u, 0u);
    WR8(v5 + 3u, r);

    if (RDS8(0x800DAE58u) == 0) {
        return 0u;
    }
    addr = s4 + 4u * (uint32_t)RDS8(s3 + 0x136u) + 0x28u;
    WR32(addr, RD8(addr));                  /* sw v1,0x28(v0): zero-extends the byte over the word */
    r = RD32(addr) | 0x99999900u;
    WR32(addr, r);
    return r;
}
