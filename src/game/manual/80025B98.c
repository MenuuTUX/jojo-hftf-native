#include "manual.h"
#include "hex2c_rt.h"

/* Original: 80025B98 (TEXT, 36 bytes). Calls 800116F8 with a0 = 0x8006D1F0 (lui 0x8007, addiu -11792) and returns its $v0.
 * a0 is overwritten before any read. a1..a3 are not set here: they are still the incoming values at the jal, so they are
 * passed on unchanged (nargs 4). 800116F8 has no manual file, so it is called with m->call4. Frame saves only $ra. */
uint32_t man_80025B98(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    return m->call4(m->ctx, 0x800116F8u, 0x8006D1F0u, a1, a2, a3);
}
