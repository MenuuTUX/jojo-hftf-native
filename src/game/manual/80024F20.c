#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_80024FD0(const GuestMem *m, uint32_t a0);
uint32_t man_800250F8(const GuestMem *m, uint32_t a0);
uint32_t man_80025194(const GuestMem *m, uint32_t a0);

/* Original: 80024F20 (TEXT, 100 bytes). Zeroes 0xF3C bytes at 0x800782B8, calls 80024FD0 with 0, 1, 2, calls 800250F8
 * with 0, then returns the $v0 of 80025194(0). Its frame only saves $ra. The incoming a1..a3 are still in place at the
 * first jal (to 80024FD0), which ignores them (its nargs is 1), so they are counted as pass-through (nargs 4) and not used. */
uint32_t man_80024F20(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = 0x800782B8u;           /* lui v0,0x8007; addiu v0,v0,0x7E98; addiu a0,v0,0x420 */
    uint32_t v1 = 0u;

    do {
        WR8(p, 0u);                     /* sb zero,0(a0) */
        v1 += 1u;                       /* addiu v1,v1,1 */
        p += 1u;                        /* delay slot: addiu a0,a0,1 */
    } while (v1 < 0xF3Cu);              /* sltiu v0,v1,3900; bne v0,zero */

    man_80024FD0(m, 0u);                /* jal 80024fd0 with a0 = 0 */
    man_80024FD0(m, 1u);                /* jal 80024fd0 with a0 = 1 */
    man_80024FD0(m, 2u);                /* jal 80024fd0 with a0 = 2 */
    man_800250F8(m, 0u);                /* jal 800250f8 with a0 = 0 (returns 127, overwritten below) */
    return man_80025194(m, 0u);         /* jal 80025194 with a0 = 0; its $v0 is what the original returns */
}
