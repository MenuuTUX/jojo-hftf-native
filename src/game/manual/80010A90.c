#include "manual.h"
#include "hex2c_rt.h"

/* Boot thread (slot 0's first entry, created by main): waits one frame, then a small state machine on the record's +0x48:
 * 0 = set display mask, start-up work (0x80010BB4, 0x80024DC4), step to 1; 1 = clear 0x80066E18, step to 3; 3 = reset the
 * record's +0x48..+0x50 and two flags, then jump the thread to the title flow (0x80011334(0, 1), does not return); waits one frame
 * per pass. Never returns. */
uint32_t man_80010A90(const GuestMem *m) {
    uint32_t p, v;
    m->call4(m->ctx, 0x80010914u, 1, 0, 0, 0);          /* wait 1 frame */
    WR16(RD32(0x80066294u) + 0x48u, 0);
    for (;;) {
        p = RD32(0x80066294u);
        v = RD16(p + 0x48u);
        if (v == 1u) {
            WR16(p + 0x48u, 2u);
            WR8(0x80066E18u, 0);
            WR16(p + 0x48u, 3u);
        } else if (v == 0u) {
            WR32(0x80055E5Cu, 1u);
            m->call4(m->ctx, 0x80036004u, 1, 0, 0, 0);     /* SetDispMask(1) */
            m->call4(m->ctx, 0x80010BB4u, 1, 0, 0, 0);
            m->call4(m->ctx, 0x80024DC4u, 1, 0, 0, 0);
            p = RD32(0x80066294u);
            WR16(p + 0x48u, RD16(p + 0x48u) + 1u);
        } else if (v == 3u) {
            WR32(0x80055E5Cu, 0);
            WR8(0x80066C65u, 0);
            WR16(p + 0x50u, 0); WR16(p + 0x4Eu, 0); WR16(p + 0x4Cu, 0); WR16(p + 0x4Au, 0); WR16(p + 0x48u, 0);
            WR8(0x80066C68u, 0);
            m->call4(m->ctx, 0x80011334u, 0, 1, 0, 0);     /* jump the thread (does not return in the original) */
        }
        m->call4(m->ctx, 0x80010914u, 1, 0, 0, 0);      /* wait 1 frame */
    }
}
