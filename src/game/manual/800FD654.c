#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FD654(const GuestMem *m, uint32_t a0, uint32_t in_v0) {
    uint32_t s0 = a0;
    uint32_t s1;
    uint32_t result;

    s1 = RD32(s0 + 0x80u);
    /* 800FD66C: bne v0,zero,800FD6BC returns with $v0 unchanged */
    if (in_v0 != 0) {
        return in_v0;
    }
    /* 800FD674: jal 80026A04 with $a0 still the original argument */
    m->call4(m->ctx, 0x80026A04u, a0, 0u, 0u, 0u);
    if (RDS8(s0 + 0x4Du) != -1 && RD8(s1 + 6u) == 2u) {
        /* 800FD69C..800FD6A4: returns the byte in $v0 when it is nonzero */
        result = RD8(s1 + 0x205u);
        if (result != 0u) {
            return result;
        }
    }
    result = RD8(s0 + 5u) + 1u;
    WR8(s0 + 5u, result);
    return result;
}
