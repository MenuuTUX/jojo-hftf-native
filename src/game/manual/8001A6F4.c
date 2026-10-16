#include "manual.h"
#include "hex2c_rt.h"

/* Segment TEXT. Reads a0 (byte index into the halfword table at 0x80059EFC), calls 8001ED60 with that halfword and zeros, then
 * builds the RECT {x=0, y=480, w=384, h=8} in the original frame at sp+0x10, calls LoadImage (80036348) with it and
 * 0x8010D800, and returns the $v0 of 80027414 (mode 2, the RECT, 0x8010D800). The original frame is 0x20 bytes. */
uint32_t man_8001A6F4(const GuestMem *m, uint32_t a0) {
    uint32_t sp = m->sp_get(m->ctx) - 0x20u;   /* 0x20 = the original "addiu sp,sp,-32" */
    uint32_t h, r;

    m->sp_set(m->ctx, sp);

    h = RD16(0x80059EFCu + ((a0 & 0xFFu) << 1));
    m->call4(m->ctx, 0x8001ED60u, h, 0u, 0u, 0u);

    WR16(sp + 0x10u, 0u);                      /* rect.x */
    WR16(sp + 0x12u, 480u);                    /* rect.y */
    WR16(sp + 0x14u, 384u);                    /* rect.w */
    WR16(sp + 0x16u, 8u);                      /* rect.h */
    m->call4(m->ctx, 0x80036348u, sp + 0x10u, 0x8010D800u, 0u, 0u);

    r = m->call4(m->ctx, 0x80027414u, 2u, sp + 0x10u, 0x8010D800u, 0u);

    m->sp_set(m->ctx, sp + 0x20u);
    return r;
}
