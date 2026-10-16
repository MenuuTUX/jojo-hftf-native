#include "manual.h"
#include "hex2c_rt.h"

/* Segment TEXT. Sets 0x800797B8 = 0x80115800, calls 8001ED60(0x268, 0, 0, 0), LoadImage and 80027414 on the RECT
 * {0, 480, 384, 8} with 0x8010A000, then sets three bytes and words at 0x80089F50..0x80089FBC, LoadImage and 80027414 on the
 * RECT {0, 488, 384, 18} with 0x8010D800, copies three words from 0x80079618..0x80079694 into the globals, and returns 44.
 * The original frame is 0x30 bytes; the RECT is at sp+0x10. */
uint32_t man_8001D194(const GuestMem *m) {
    uint32_t sp = m->sp_get(m->ctx) - 0x30u;   /* 0x30 = the original "addiu sp,sp,-48" */
    uint32_t w1, w2, w3, w4;

    m->sp_set(m->ctx, sp);

    WR32(0x800797B8u, 0x80115800u);            /* delay slot of the first jal in the original: stored before the call */
    m->call4(m->ctx, 0x8001ED60u, 0x268u, 0u, 0u, 0u);

    WR16(sp + 0x10u, 0u);                      /* rect.x */
    WR16(sp + 0x12u, 480u);                    /* rect.y */
    WR16(sp + 0x14u, 384u);                    /* rect.w */
    WR16(sp + 0x16u, 8u);                      /* rect.h */
    m->call4(m->ctx, 0x80036348u, sp + 0x10u, 0x8010A000u, 0u, 0u);
    m->call4(m->ctx, 0x80027414u, 2u, sp + 0x10u, 0x8010A000u, 0u);

    WR8(0x80089F50u, 24u);
    WR32(0x80089FB0u, 0u);
    WR8(0x80089F51u, 16u);

    WR16(sp + 0x10u, 0u);                      /* rect.x */
    WR16(sp + 0x12u, 488u);                    /* rect.y */
    WR16(sp + 0x14u, 384u);                    /* rect.w (s0 still holds 384 here) */
    WR16(sp + 0x16u, 18u);                     /* rect.h */
    WR32(0x80089F70u, RD32(0x80079618u));
    m->call4(m->ctx, 0x80036348u, sp + 0x10u, 0x8010D800u, 0u, 0u);
    m->call4(m->ctx, 0x80027414u, 2u, sp + 0x10u, 0x8010D800u, 0u);

    WR32(0x80089FBCu, 0u);

    w1 = RD32(0x8007961Cu);
    w2 = RD32(0x80079620u);
    w3 = RD32(0x80079694u);
    WR8(0x80089F56u, 44u);
    WR8(0x80089F57u, 0u);
    WR8(0x80089F54u, 0u);
    WR8(0x80089F55u, 0u);
    WR32(0x80089F7Cu, w1);
    WR32(0x80089FB8u, w2);
    WR32(0x80089F78u, w3);
    w4 = 44u;

    m->sp_set(m->ctx, sp + 0x30u);
    return w4;
}
