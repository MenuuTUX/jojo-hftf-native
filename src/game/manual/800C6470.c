#include "manual.h"
#include "hex2c_rt.h"

/* 800C6470 (segment AC_MAIN, 180 bytes). Frame 0x18: ra is stored at sp+0x10 (sw ra) and reloaded before jr.
 * a0 = object record. The MIPS copies it to $a1 first (addu a1,a0,zero at 800C6474) and nothing writes $a1 again, so the
 * jal 800C6524 at 800C650C gets a0 = object (set in its delay slot) and a1 = object (the copy). $a2 and $a3 are never
 * written by this function, so they pass on as the incoming values.
 * Fills the object fields, then picks the byte at +7 and halfword at +14 from one of two tables: the branch on the byte at +3
 * (800C64B8 bne) selects the 0x800CE217/0x800CE2F4 pair when it is non-zero, else the 0x800CDE87/0x800CDF64 pair.
 * lb (sign-extended) is used for the first table of each pair, lbu for the second. Returns $v0 as the callee leaves it. */
uint32_t man_800C6470(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx);
    uint32_t obj = a0;                    /* $a1 copy of the incoming $a0 */
    uint32_t entry_ra = m->gpr_get(m->ctx, 31u);
    uint32_t base = 0x800CDD50u;          /* lui 0x800d / addiu -8880 */
    uint32_t v0, v1, a0v, result;
    uint32_t frame = sp - 24u;

    (void)a1;                             /* incoming $a1 is overwritten by the copy above */
    m->sp_set(m->ctx, frame);
    WR32(frame + 0x10u, entry_ra);        /* sw ra,0x10(sp) */

    WR8(obj + 0x1Du, 5u);
    WR8(obj + 0x01u, 1u);
    WR16(obj + 0x12u, 192u);
    WR16(obj + 0x16u, 216u);
    WR8(obj + 0x06u, 0xFFu);
    WR8(obj + 0x05u, 40u);
    v0 = (uint32_t)RD8(obj + 0x08u) + 1u;  /* ++*(obj+8) */
    WR8(obj + 0x08u, v0);
    v1 = RD8(obj + 0x03u);

    if (v1 != 0u) {
        /* 800C64E4: v6 = 2*byte_800CE217; v4 = byte_800CE2F4 + v6 + 83; v5 = v6 + 44 */
        a0v = (uint32_t)(int32_t)(int8_t)RD8(base + 0x4C7u);
        v0 = RD8(base + 0x5A4u);
        a0v = a0v << 1;
        v1 = a0v + 83u;
        v0 = v0 + v1;
        a0v = a0v + 44u;
    } else {
        /* 800C64C0: v3 = 2*byte_800CDE87; v4 = byte_800CDF64 + v3 + 84; v5 = v3 + 43 */
        a0v = (uint32_t)(int32_t)(int8_t)RD8(base + 0x137u);
        v0 = RD8(base + 0x214u);
        a0v = a0v << 1;
        v1 = a0v + 84u;
        v0 = v0 + v1;
        a0v = a0v + 43u;
    }
    WR8(obj + 0x07u, v0);
    WR16(obj + 0x0Eu, a0v);

    /* 800C650C jal 800C6524; a0 = obj (delay slot addu a0,a1,zero), a1 = obj (copy of incoming a0), a2/a3 unchanged */
    result = m->call4(m->ctx, 0x800C6524u, obj, obj, a2, a3);

    m->sp_set(m->ctx, sp);
    return result;
}
