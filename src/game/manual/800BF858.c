#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800BF858(const GuestMem *m, uint32_t a0) {
    uint32_t p = a0;
    uint32_t a2 = 0;
    int32_t sv = RDS8(p + 0x137);

    if (sv != 13 && sv != 21) {
        m->call4(m->ctx, 0x80024564u, p, 0, 0, 0);
        m->call4(m->ctx, 0x8002B450u, 4, 2, 0, 0);
        m->call4(m->ctx, 0x800C7374u, p, 0, 0, 0);
        m->call4(m->ctx, 0x80029714u, p, 14, 8, 0);
        a2 = 8;                             /* addiu a2,zero,8: delay slot of jal 80029714 */
    }
    WR16(p + 0x6E, 35);                     /* $v0 = 35 on every path; sh is the delay slot of jal 800bcc04 */
    return m->call4(m->ctx, 0x800BCC04u, p, 65, a2, 0);
}
