#include "manual.h"
#include "hex2c_rt.h"

/* TR_SEL 800DFA7C..800DFAD3 (88 bytes). No arguments, no calls. Reads the record at 0x800E2A58: returns 1 when the signed
 * byte at +5 is 11, 14 or 17, or when the signed byte at +0x25 is 11, 14 or 17 (the lbu of +0x25 runs before any branch);
 * otherwise returns 0. */
uint32_t man_TR_SEL_800DFA7C(const GuestMem *m) {
    const uint32_t rec = 0x800E2A58u;
    int32_t v1 = RDS8(rec + 5u);                         /* lb v1,5(v0) at 800dfa88 */
    int32_t v0 = RDS8(rec + 0x25u);                      /* lbu v0,0x25(v0) at 800dfa8c; sll/sra 24 = signed byte */

    if (v1 == 11 || v1 == 14 || v1 == 17) return 1u;     /* beq chain to 800dfac4 (delay slot addiu v0,zero,1) */
    if (v0 == 11 || v0 == 14 || v0 == 17) return 1u;
    return 0u;                                           /* bne v0,a0,800dfacc: addu v0,zero,zero in its delay slot */
}
