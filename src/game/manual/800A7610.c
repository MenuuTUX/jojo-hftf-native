#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_80025280(const GuestMem *m);

/* 800A7610 (AC_MAIN). Calls 80025280 and returns its result. Reads no argument register (nargs 0). The original passes
 * the callee's 64-bit result through in $v0:$v1; this interface returns only $v0. */
uint32_t man_800A7610(const GuestMem *m) {
    return man_80025280(m);
}
