#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800FD654(const GuestMem *m, uint32_t a0, uint32_t in_v0);

/* 800FD64C loads $v0 = byte at 0x80066CA2 and falls through into 800FD654 with $a0 unchanged. */
uint32_t man_800FD64C(const GuestMem *m, uint32_t a0) {
    return man_800FD654(m, a0, RD8(0x80066CA2u));
}
