#include "manual.h"
#include "hex2c_rt.h"

/* PL01 800E2AB8..800E2B68 (176 bytes, slot A base 800DF000). A chain of calls, each with $a0 = the object and $a1, $a2,
 * $a3 passed on unchanged: 800E2F44, 800E2ED4, 800E2DF4, 800E2D84, 800E2E70, 800E2D04, 800E2C84, 800E2C20. The first one
 * with a nonzero $v0 returns 1 (the delay slot of each bne sets $v0 = 1). If all are zero, the result is
 * (800E2BC4(a0, ...) != 0) via sltu v0,zero,v0 in a delay slot. The generated C passed 0 for $a1..$a3 on every call. */
uint32_t man_PL01_800E2AB8(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    if (m->call4(m->ctx, 0x800E2F44u, a0, a1, a2, a3) != 0u) return 1u;   /* jal 800e2f44; bne v0,zero,800e2b58 */
    if (m->call4(m->ctx, 0x800E2ED4u, a0, a1, a2, a3) != 0u) return 1u;   /* jal 800e2ed4 */
    if (m->call4(m->ctx, 0x800E2DF4u, a0, a1, a2, a3) != 0u) return 1u;   /* jal 800e2df4 */
    if (m->call4(m->ctx, 0x800E2D84u, a0, a1, a2, a3) != 0u) return 1u;   /* jal 800e2d84 */
    if (m->call4(m->ctx, 0x800E2E70u, a0, a1, a2, a3) != 0u) return 1u;   /* jal 800e2e70 */
    if (m->call4(m->ctx, 0x800E2D04u, a0, a1, a2, a3) != 0u) return 1u;   /* jal 800e2d04 */
    if (m->call4(m->ctx, 0x800E2C84u, a0, a1, a2, a3) != 0u) return 1u;   /* jal 800e2c84 */
    if (m->call4(m->ctx, 0x800E2C20u, a0, a1, a2, a3) != 0u) return 1u;   /* jal 800e2c20; bne v0,zero,800e2b54 -> v0 = 1 */
    return (m->call4(m->ctx, 0x800E2BC4u, a0, a1, a2, a3) != 0u) ? 1u : 0u;   /* jal 800e2bc4; sltu v0,zero,v0 */
}
