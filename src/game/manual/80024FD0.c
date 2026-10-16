#include "manual.h"
#include "hex2c_rt.h"

/* Original: 80024FD0 (TEXT, 296 bytes). $a0 masked to 8 bits selects the table: 3 means base = 0x80066764 + 32*i;
 * any other value means base = 0x800782EC + 1300*sel + 32*i. For i in 0..38: byte 11 of base = 5, then a switch on i
 * stores 0xFF or 1 at p = base + 25 (p+0..p+3). Returns 3 (the loop's delay-slot addiu v0,zero,3). */
uint32_t man_80024FD0(const GuestMem *m, uint32_t a0) {
    uint32_t sel = a0 & 0xFFu;          /* andi a0,a0,0xff */
    uint32_t i;
    uint32_t base;
    uint32_t p;

    for (i = 0; i < 0x27u; i++) {
        if (sel == 3u) {
            base = 0x80066764u + (i << 5);
        } else {
            base = 0x800782ECu + sel * 1300u + (i << 5);
        }
        p = base + 25u;                 /* addiu a2,a1,25 (and t3 = 0x8006677D + 32i = base + 25 for sel == 3) */
        WR8(base + 11u, 5u);            /* sb v0,0xb(a1) */

        switch (i) {
        case 4u:
        case 9u:
        case 10u:
        case 18u:
        case 29u:
            WR8(p + 1u, 0xFFu);
            break;
        case 5u:
            WR8(p + 2u, 0xFFu);
            break;
        case 11u:
        case 12u:
            break;
        case 13u:
            WR8(p + 0u, 0xFFu);
            WR8(p + 3u, 1u);
            break;
        case 15u:
        case 17u:
            break;
        case 24u:
            WR8(p + 0u, 0xFFu);
            WR8(p + 1u, 0xFFu);
            WR8(p + 2u, 0xFFu);
            break;
        case 27u:
            WR8(p + 0u, 1u);
            break;
        default:
            break;
        }
    }
    return 3u;                          /* jr ra with $v0 = 3 */
}
