#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800A23B4(const GuestMem *m, uint32_t a0, uint32_t a1);

/*
 * 800A227C falls through into 800A2364 (80 bytes, same stack frame) on the v1 == 2 path.
 * That code is inlined below, because a call would not see this frame's ra/sp.
 * The jumps to 800A23A4 land in the epilogue of 800A2364, so they are plain returns here.
 */
uint32_t man_800A227C(const GuestMem *m, uint32_t a0) {
    uint32_t a1 = a0;                       /* addu a1,a0,zero at 800A2280 */
    uint32_t p = RD32(0x800A34E4u);         /* lw a0,0x34e4(s0), s0 = 0x800A0000 */
    uint32_t v1 = RD8(p + 6u);
    uint32_t s;
    uint32_t r;
    uint32_t b;

    if (v1 == 1u) {
        /* 800A22E8: jal 80025D40, then check 12289 at the current entry */
        r = m->call4(m->ctx, 0x80025D40u, p, a1, 1u, 0u);
        if ((r & 0xFFFFu) != 0u) {
            s = RD32(0x800F89B8u);
            WR16(s + 14u, 0u);
            WR16(s + 12u, 0u);
        }
        s = RD32(0x800F89B8u);
        v1 = RD16(RD32(s + 64u) + (RD8(s + 11u) << 1));
        if (v1 != 0x3001u) {
            return 0u;
        }
        WR8(p + 6u, RD8(p + 6u) + 1u);
        WR8(p + 7u, 90u);
        return 0u;                          /* j 800A23A0, addu v0,zero,zero */
    }

    if (v1 == 0u) {
        /* 800A22CC: sb a2 (=1) to P[6], then call 800A23B4 with a1 & 0xFF; $v0 forced to 0 after */
        WR8(p + 6u, 1u);
        man_800A23B4(m, 0x800F89C0u, a1 & 0xFFu);
        return 0u;
    }

    if (v1 == 2u) {
        /* 800A2360 falls into 800A2364: decrement P[7], maybe call 80025D40, then P[6] = 0 and return 1 */
        b = RD8(p + 7u) - 1u;
        WR8(p + 7u, b);
        if ((b & 0xFFu) != 0u) {
            r = m->call4(m->ctx, 0x80025D40u, p, a1, 1u, 0u);
            if ((r & 0xFFFFu) == 0u) {
                return 0u;
            }
        }
        WR8(p + 6u, 0u);
        return 1u;
    }

    return 0u;                              /* 800A22C0: v1 >= 3 */
}
