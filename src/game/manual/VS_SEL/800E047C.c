#include "manual.h"
#include "hex2c_rt.h"

/* VS_SEL 800E047C..800E04B8. Dispatcher: addiu sp,-24; v0 = table[byte (a0+5)] at 0x800E23BC; jalr ra,v0 with $a0..$a3
 * as received; return the callee's $v0. The generated C returned the table entry (a code address) and dropped the jalr:
 * Hex-Rays showed only "return *(sub_800E23BC + byte)". calli sets $v0 = target at entry, as jalr $v0 does. */
uint32_t man_VS_SEL_800E047C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx) - 24u;               /* addiu sp,sp,-24 */
    uint32_t fn, r;
    m->sp_set(m->ctx, sp);
    fn = RD32(0x800E23BCu + 4u * (uint32_t)RD8(a0 + 5u));  /* lbu, sll 2, addu, lw */
    r = m->calli(m->ctx, fn, a0, a1, a2, a3);             /* jalr ra,v0 (delay slot nop) */
    m->sp_set(m->ctx, sp + 24u);                          /* addiu sp,sp,24 in the epilogue */
    return r;
}
