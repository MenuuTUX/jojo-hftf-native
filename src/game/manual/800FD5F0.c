#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FD5F8(const GuestMem *m, uint32_t a0, uint32_t in_v0);

/* 800FD5F0 loads $v0 = byte at 0x80066CA2 and falls through into 800FD5F8 with $a0 unchanged. */
uint32_t man_800FD5F0(const GuestMem *m, uint32_t a0) {
    return man_800FD5F8(m, a0, RD8(0x80066CA2u));
}
