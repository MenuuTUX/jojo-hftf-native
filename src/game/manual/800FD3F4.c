#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FD3FC(const GuestMem *m, uint32_t a0, uint32_t in_v0);

/* 800FD3F4 loads $v0 = byte at 0x80066CA2 and falls through into 800FD3FC with $a0 unchanged. */
uint32_t man_800FD3F4(const GuestMem *m, uint32_t a0) {
    return man_800FD3FC(m, a0, RD8(0x80066CA2u));
}
