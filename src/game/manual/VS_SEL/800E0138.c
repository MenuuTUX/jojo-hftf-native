#include "manual.h"
#include "hex2c_rt.h"

/* VS_SEL 800E0138..800E0190. Leaf, no arguments, no calls. Bit 0 of the byte at 0x80066C6B selects v1 = (halfword 0x8006CEBE & 0xF0) != 0;
 * bit 1 then ORs in 2 when the halfword at 0x8006CECE & 0xF0 is non-zero. The generated C ran the 0x8006CEBE test with no flag
 * bit and OR'd in 2 on bit 1 with no 0x8006CECE test, so it returned 2 where the original returns 0. */
uint32_t man_VS_SEL_800E0138(const GuestMem *m) {
    uint32_t flags = RD8(0x80066C6Bu);                    /* lbu a0,0x6c6b(v0) */
    uint32_t v1 = 0u;                                     /* addu v1,zero,zero (delay slot, both paths) */
    if ((flags & 1u) != 0)
        v1 = (RD16(0x8006CEBEu) & 0xF0u) != 0 ? 1u : 0u;  /* lhu, andi 0xf0, sltu */
    if ((flags & 2u) != 0) {
        if ((RD16(0x8006CECEu) & 0xF0u) != 0)             /* lhu at -0x3132 from 0x80070000, andi 0xf0 */
            v1 |= 2u;                                     /* ori v1,v1,2 */
    }
    return v1 & 0xFFFFu;                                  /* andi v0,v1,0xffff in the jr delay slot */
}
