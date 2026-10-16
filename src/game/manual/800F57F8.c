#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800F5800(const GuestMem *m, uint32_t a0, uint32_t in_v0);

/* 800F57F8: $v0 = sign-extended byte at 0x80066CC4, then falls into 800F5800 with $a0 unchanged. */
uint32_t man_800F57F8(const GuestMem *m, uint32_t a0) {
    uint32_t v0 = (uint32_t)RDS8(0x80066CC4u);
    return man_800F5800(m, a0, v0);
}
