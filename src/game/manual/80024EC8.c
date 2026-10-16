#include "manual.h"
#include "hex2c_rt.h"

/* Original: 80024EC8 (TEXT, 40 bytes). Zeroes the 0x50 bytes at 0x80066C48, one byte store per iteration.
 * No inputs. Returns 0: the last sltiu leaves $v0 = (0x50 < 0x50) = 0. */
uint32_t man_80024EC8(const GuestMem *m) {
    uint32_t a0 = 0x80066C48u;          /* lui v0,0x8006; addiu a0,v0,0x6C48 */
    uint32_t v1 = 0u;
    uint32_t v0;

    do {
        WR8(a0, 0u);                    /* sb zero,0(a0) */
        v1 += 1u;                       /* addiu v1,v1,1 */
        v0 = (v1 < 0x50u) ? 1u : 0u;    /* sltiu v0,v1,80 */
        a0 += 1u;                       /* delay slot of bne: addiu a0,a0,1 */
    } while (v0 != 0u);                 /* bne v0,zero,80024ed4 */
    return v0;                          /* jr ra with $v0 */
}
