#include "manual.h"
#include "hex2c_rt.h"

/* TR_SEL 800DFFE8..800E003B (88 bytes). $a0 = P, $a1 = output byte pointer Q. No calls. Reads halfword P+0x10 first, then
 * always stores 0 to Q (the delay slot of the beq at 800dfff4). Bit 0x8000 stores 3 and skips the 0x2000 test; otherwise
 * bit 0x2000 stores 4. Bit 0x1000 stores 1 and returns 1. Otherwise bit 0x4000 stores 2 and the function returns 2 (the
 * addiu v0,zero,2 at 800e0030 is in the delay slot of the beq at 800e002c, so it runs whether or not the branch is taken). */
uint32_t man_TR_SEL_800DFFE8(const GuestMem *m, uint32_t a0, uint32_t a1) {
    uint32_t p = a0, q = a1;
    uint32_t v1 = RD16(p + 0x10u);                       /* lhu v1,0x10(a0) at 800dffe8 */

    WR8(q, 0u);                                          /* sb zero,0(a1): delay slot of beq at 800dfff4 */
    if (v1 & 0x8000u) {
        WR8(q, 3u);                                      /* j 800e0010 with addiu v0,zero,3 in its delay slot */
    } else if (v1 & 0x2000u) {
        WR8(q, 4u);                                      /* addiu v0,zero,4 (delay of beq at 800e0008) then sb at 800e0010 */
    }

    if (v1 & 0x1000u) {
        WR8(q, 1u);                                      /* addiu v0,zero,1; jr ra with sb in its delay slot */
        return 1u;
    }
    if (v1 & 0x4000u) WR8(q, 2u);                        /* sb v0,0(a1) at 800e0034 (taken-path beq skips it) */
    return 2u;
}
