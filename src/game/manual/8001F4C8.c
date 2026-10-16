#include "manual.h"
#include "hex2c_rt.h"

/* 8001F4C8: builds the GPU primitives of one sprite object. $a0 = the output object (write pointer at +0x44, copied to
 * +4 on return), $a1 = the source record. The command list at src+0x40 is read as halfwords: 0x3000 and 0x3003 are skip
 * commands, 0 is an empty cell, anything else is a sprite (written into the primitive buffer at
 * 0x80089DC8 + 64*idx + 16*w, linked into the OT through the tag word V8 = 0x80089D68 + 48*idx + 12*w).
 * idx = 0x80066E14, w = 0x80081BCC (incremented at the end). ClearOTag (0x800364C0) runs on V8 with ($a1, $a2, $a3) =
 * (3, idx, w). Frame: saves only, no local memory. Returns the write pointer.
 * Registers that carry across the loop: $a2 (command counter, wraps), $a3 (cell counter, tested as a halfword against
 * src[11]), $s3 (command pointer). A full primitive buffer (write pointer at or past 0x8007D3C8 + 15360*idx - 20) leaves
 * the loop at once, skipping the tail updates, as the beq at 8001F81C does. */
uint32_t man_8001F4C8(const GuestMem *m, uint32_t a0, uint32_t a1) {
    uint32_t obj = a0;
    uint32_t src = a1;
    uint32_t idx, w, v8, p, a2c, a3c, x, q, rem, off, c, limit, cur, prim, a0v, t2v, a3v, qv, base, addr, otv;
    uint32_t v0v, v1v;

    WR32(obj + 0x44u, RD32(obj + 4u));

    if (RD8(src) != 0u && RD8(src + 1u) != 0u) {
        WR16(obj + 0x32u, RD16(src + 0x6Au));
        WR16(obj + 0x34u, 240u - RD16(src + 0x6Eu));

        idx = RD32(0x80066E14u);
        w = RD16(0x80081BCCu);

        /* first primitive header: word 1 and word 3 are set here, the tag-word link is written after the ClearOTag call */
        prim = 0x80089DC8u + (idx << 6) + (w << 4);
        WR8(prim + 3u, 1u);
        WR32(prim + 4u, (((RD8(src + 0xB3u) & 0x10u) + (RD8(src + 0xB2u) >> 2)) | 0xE1000200u));
        WR8(prim + 0xBu, 1u);
        WR32(prim + 0xCu, (((RD8(src + 0xB2u) >> 2) + (RD8(src + 0xB3u) & 0x10u) + 1u) | 0xE1000200u));

        v8 = 0x80089D68u + idx * 48u + w * 12u;
        m->call4(m->ctx, 0x800364C0u, v8, 3u, idx, w);

        a2c = 0u;
        a3c = 0u;
        if (RD8(src + 0xBu) != 0u) {
            p = RD32(src + 0x40u);
            for (;;) {
                uint32_t cmd = RD16(p);
                if (cmd == 0x3000u) {
                    WR16(obj + 0x34u, RD16(obj + 0x34u) + 16u);
                    a2c -= 1u;
                    WR16(obj + 0x32u, RD16(src + 0x6Au));
                } else if (cmd == 0x3003u) {
                    a2c -= 1u;
                    a3c += 1u;
                    p += 2u;               /* the delay-slot increment at 8001F684 */
                } else if (cmd == 0u) {
                    a2c -= 1u;
                    WR16(obj + 0x32u, RD16(obj + 0x32u) + 8u);
                } else {
                    /* sprite */
                    WR8(RD32(obj + 0x44u) + 3u, 4u);
                    WR8(RD32(obj + 0x44u) + 7u, 100u);
                    WR8(RD32(obj + 0x44u) + 7u, 100u);
                    WR8(RD32(obj + 0x44u) + 0xCu, (a2c & 0x1Fu) << 3);
                    WR8(RD32(obj + 0x44u) + 0xDu, ((a2c & 0xE0u) >> 1) + (RD8(src + 0xB3u) << 4));
                    WR16(RD32(obj + 0x44u) + 8u, RD16(obj + 0x32u));
                    WR16(RD32(obj + 0x44u) + 0xAu, RD16(obj + 0x34u));

                    x = RD16(src + 0x1Eu);
                    q = (uint32_t)(((uint64_t)x * 0xAAAAAAABu) >> 32) >> 4;   /* x / 24 */
                    rem = x - q * 24u;                                         /* x % 24 */
                    WR16(RD32(obj + 0x44u) + 0xEu, ((RD16(obj + 0x90u) + q) << 6) | (rem & 0x3Fu));

                    off = rem + 24u * (RD16(obj + 0x90u) + (q & 0xFFFFu) - 480u);
                    c = RD8(0x8006CEE0u + off);
                    WR8(RD32(obj + 0x44u) + 6u, c);
                    WR8(RD32(obj + 0x44u) + 5u, c);
                    WR8(RD32(obj + 0x44u) + 4u, c);
                    WR16(RD32(obj + 0x44u) + 0x10u, 8u);
                    WR16(RD32(obj + 0x44u) + 0x12u, 16u);

                    cur = RD32(obj + 0x44u);
                    WR32(cur, (RD32(cur) & 0xFF000000u) | (RD32(v8) & 0xFFFFFFu));
                    WR32(v8, (RD32(v8) & 0xFF000000u) | (RD32(obj + 0x44u) & 0xFFFFFFu));

                    cur = RD32(obj + 0x44u);
                    limit = 0x8007D3C8u + 15360u * idx - 20u;
                    if (!(cur < limit)) {
                        break;             /* beq at 8001F81C: to the epilogue, no tail */
                    }
                    WR32(obj + 0x44u, cur + 20u);
                    WR16(obj + 0x32u, RD16(obj + 0x32u) + 8u);
                }
                p += 2u;
                a3c += 1u;
                a2c += 1u;                 /* delay slot of the loop branch: runs on exit too */
                if (!((a3c & 0xFFFFu) < RD8(src + 0xBu))) {
                    break;
                }
            }
        }

        /* epilogue of the body (8001F854): the second primitive, links into the tag words and the OT */
        idx = RD32(0x80066E14u);
        w = RD16(0x80081BCCu);
        a0v = idx << 6;
        t2v = w << 4;
        a3v = a0v + t2v;
        prim = a3v + 0x80089DC8u;
        v0v = RD32(v8);
        v1v = RD32(prim);
        v1v = (v1v & 0xFF000000u) | (v0v & 0xFFFFFFu);
        WR32(prim, v1v);

        qv = a0v + t2v + 0x80089DD0u;
        WR32(v8, (RD32(v8) & 0xFF000000u) | (prim & 0xFFFFFFu));
        WR32(qv, (RD32(qv) & 0xFF000000u) | (RD32(v8 + 4u) & 0xFFFFFFu));
        WR32(v8 + 4u, (RD32(v8 + 4u) & 0xFF000000u) | (qv & 0xFFFFFFu));

        base = RD32(0x80066E10u);
        otv = RD32(base + (RD8(src + 0x32u) << 2));
        WR32(v8 + 8u, (RD32(v8 + 8u) & 0xFF000000u) | (otv & 0xFFFFFFu));

        addr = base + (RD8(src + 0x32u) << 2);
        otv = RD32(addr);
        WR16(0x80081BCCu, w + 1u);
        WR32(addr, (otv & 0xFF000000u) | (v8 & 0xFFFFFFu));
    }

    WR32(obj + 4u, RD32(obj + 0x44u));
    return RD32(obj + 0x44u);
}
