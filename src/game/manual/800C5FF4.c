#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800C6060(const GuestMem *m, uint32_t a0);

uint32_t man_800C5FF4(const GuestMem *m, uint32_t a0) {
    uint32_t v0, a1;

    WR8(a0 + 0x1D, 5);
    WR8(a0 + 1, 1);
    WR8(a0 + 7, 74);
    WR8(a0 + 6, 0xFF);
    v0 = RD8(a0 + 8);
    WR8(a0 + 5, 39);
    WR8(a0 + 8, v0 + 1);
    a1 = 14;
    if ((int32_t)RDS8(0x8006CE51u) == 3) {
        a1 = 15;
    }
    WR16(a0 + 0xE, a1);                     /* sh a1,0xe(a0): delay slot of jal 800c6060 */
    return man_800C6060(m, a0);             /* $v0 is not read before written by 800c6060 */
}
