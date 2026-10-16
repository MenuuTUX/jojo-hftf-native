#include "manual.h"
#include "hex2c_rt.h"

/* 800227A8 (TEXT). $a0 = B (struct: output halfwords at +0x78 and +0x80), $a1 = S (parameter block),
 * $a2 = n (signed byte count). The loop runs n times; each pass writes one halfword pair at B+0x78+2i and B+0x80+2i. */

/* sign-extend the low 16 / 8 bits, the way lh / lb do */
#define SX16(x) ((int32_t)(((uint32_t)(x) & 0xFFFFu) ^ 0x8000u) - 0x8000)
#define SX8(x)  ((int32_t)(((uint32_t)(x) & 0xFFu) ^ 0x80u) - 0x80)

/* MIPS div: the low 32 bits of the quotient. The compiler's 'break 6' overflow trap (INT_MIN / -1) is not modelled:
 * the quotient is taken as INT_MIN, the value the divide itself yields. */
#define HX_DIV(x, y) ((uint32_t)((((int32_t)(y)) == -1 && ((uint32_t)(x)) == 0x80000000u) \
                        ? (int32_t)0x80000000u : (int32_t)(x) / (int32_t)(y)))

uint32_t man_800227A8(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2) {
    const uint32_t B = a0;
    const uint32_t S = a1;
    const int32_t n = SX8(a2);
    const uint32_t b2 = 2u * RD8(S + 0x13u);
    const uint32_t result = RD16(B + b2 + 0x54u);
    uint32_t r1 = RD16(S + 0xF2u) - RD16(B + b2 + 0x4Cu);
    uint32_t r0 = RD16(S + 0xF4u) - result;
    const int32_t t9 = SX16(r1);
    const int32_t t8 = SX16(r0);
    uint32_t wp = B;
    int32_t i = 0;

    if (n <= 0)
        return result;

    do {
        const uint32_t off = 2u * (uint32_t)i;
        const int32_t v15 = SX16(RD16(S + 0xD0u + off));
        const int32_t v16 = SX16(RD16(S + 0xD8u + off));
        const uint32_t pa = (uint32_t)v15 * ((RD16(S + 0xEEu) + 1u) << 2);
        const uint32_t pb = (uint32_t)v16 * ((RD16(S + 0xF0u) + 1u) << 2);
        const int32_t v18 = (int32_t)pa >> 8;
        const int32_t v17 = (int32_t)pb >> 8;
        const uint32_t m60 = (uint32_t)v18 * RD32(B + 0x60u);
        const uint32_t m64 = (uint32_t)v17 * RD32(B + 0x64u);
        const uint32_t m68 = (uint32_t)v18 * RD32(B + 0x68u);
        const uint32_t m6c = (uint32_t)v17 * RD32(B + 0x6Cu);
        uint32_t a1r = (uint32_t)((int32_t)(m60 + m64) >> 12);
        uint32_t a0r = (uint32_t)((int32_t)(m68 + m6c) >> 12);
        uint32_t ec;

        a1r = a1r - (uint32_t)SX16(RD16(B + b2 + 0x4Cu));
        a0r = a0r - (uint32_t)SX16(RD16(B + b2 + 0x54u));
        a1r = a1r + (uint32_t)SX16(RD16(S + 0x6Au));
        a0r = a0r + (uint32_t)SX16(RD16(S + 0x6Eu));

        ec = RD16(S + 0xECu);
        if (ec != 0u) {
            const uint32_t q70 = (uint32_t)v15 * RD32(B + 0x70u);
            const uint32_t q74 = (uint32_t)v16 * RD32(B + 0x74u);
            const int32_t q = (int32_t)(q70 + q74) >> 12;
            uint32_t d = (uint32_t)q + (uint32_t)SX16(RD16(S + 0xCAu)) + ec;

            if (d == 0u)
                d = 1u;
            a1r = a1r - (uint32_t)t9;
            a1r = HX_DIV(a1r * ec, d);
            a0r = a0r - (uint32_t)t8;
            a0r = HX_DIV(a0r * ec, d);
            a1r = a1r + (uint32_t)t9;
            a0r = a0r + (uint32_t)t8;
        }

        a1r = a1r + (uint32_t)SX16(RD16(S + 0xB4u));
        a0r = a0r + (uint32_t)SX16(RD16(S + 0xB6u));
        a0r = 240u - a0r;
        WR16(wp + 0x78u, a1r);
        WR16(wp + 0x80u, a0r);

        i = i + 1;
        wp = wp + 2u;
    } while (i < n);

    /* the last 'slt' of the loop leaves $v0 = 0 */
    return 0u;
}
