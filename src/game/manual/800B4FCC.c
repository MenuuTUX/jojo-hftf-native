#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800B4FCC(const GuestMem *m, uint32_t a0, uint32_t in_v0) {
    uint32_t v0 = in_v0;
    uint32_t a2 = RD32(a0 + 0x108);
    uint32_t v1, a1;

    if (a2 == 0) {
        return v0;                          /* beq a2,zero at 800b4fd4: $v0 untouched */
    }
    v1 = RD8(a2 + 0x1A);
    v0 = 1;                                 /* addiu v0,zero,1 in the delay slot of bne at 800b4fe4 */
    if (v1 == 2 && RD8(a2 + 0x15B) == 0) {
        return v0;                          /* beq at 800b4ff4 taken; its delay slot already set $v0 = 1 */
    }
    a1 = a0 + 0x10C;
    WR8(a1 + 2, v0);                        /* sb v0,0x2(a1) at 800b4ffc */
    WR32(a1 + 4, a2);                       /* sw a2,0x4(a1) */
    WR8(a1 + 3, 0);                         /* sb zero,0x3(a1) */
    v0 = RD8(a0 + 0x223);                   /* lbu v0,0x223(a0) */
    WR8(a1 + 1, v0);                        /* sb v0,0x1(a1) */
    return v0;
}
