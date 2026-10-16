#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800C9FE4(const GuestMem *m, uint32_t a0, uint32_t in_v0);

/* 800C9FDC loads $v0 = byte at 0x80066CB0 and falls through into 800C9FE4 with $a0 unchanged. */
uint32_t man_800C9FDC(const GuestMem *m, uint32_t a0) {
    return man_800C9FE4(m, a0, RD8(0x80066CB0u));
}
