#include "manual.h"
#include "hex2c_rt.h"

/* 80023778 (TEXT, 232 bytes): copies the source record $a1 into the destination $a0 and recomputes its position
 * and type fields. $a2 is the owner record (s0). The table entry is at S.u32[0xBC] + 4 * S.u8[0xBB]; a null entry
 * returns 4 * S.u8[0xBB] with no store.
 *
 * Hidden arguments at the jal 80020F20 (0x80023844): the pseudocode passes three, the MIPS sets four.
 *   $a0 = owner ($a2 at entry, copied to s0 at 80023798)
 *   $a1 = the word at owner+0x5C, loaded at 80023834 and tested for zero before the jal
 *   $a2 = dest ($a0 at entry), from addu a2,a3,zero in the delay slot
 *   $a3 = dest ($a0 at entry), from addu a3,a0,zero at 8002377C; no later write, so the caller's $a3 is not used
 * The result is stored to owner+0x5C and returned in $v0.
 *
 * Return ($v0): 4 * S.u8[0xBB] when the table pointer is null; 42 when owner+0x5C is zero; otherwise the
 * 80020F20 result.
 * $a3 on entry is not read. The MIPS stores s0 and ra in the frame; the callee frames sit below that, so they are
 * not emulated. */
uint32_t man_80023778(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t d = a0;            /* dest, $a3 in the MIPS */
    uint32_t s = a1;            /* source record */
    uint32_t owner = a2;        /* s0 */
    uint32_t sp;
    uint32_t v0, v1, p, t;
    uint32_t x;
    uint32_t callee_a1;
    uint32_t r;

    (void)a3;                   /* not read: the MIPS overwrites $a3 with the dest at entry */

    sp = m->sp_get(m->ctx);
    m->sp_set(m->ctx, sp - 24u);

    v0 = (uint32_t)RD8(s + 0xBBu) << 2;     /* lbu, sll 2: also the $v0 if the entry is null */
    p = RD32(s + 0xBCu) + v0;               /* lw v1,0xbc(s); addu v1,v1,v0 */
    if (p == 0u) {
        goto done;
    }

    WR8(d + 0x0Bu, RD8(s + 0x0Bu));
    WR8(d + 0x12u, RD8(s + 0x12u));
    WR16(d + 0x10u, RD16(p));

    x = RD16(s + 0x6Au);                    /* lhu a0,0x6a(s) */
    v0 = RD8(d + 0x0Bu);                    /* re-read dest byte just written */
    WR16(d + 0x6Au, x);                     /* sh a0,0x6a(dest): delay slot of the beq, runs on both paths */
    if (v0 != 0u) {
        v0 = x - RD16(p + 2u);              /* subu v0,a0,v0 */
    } else {
        v0 = x + RD16(p + 2u);              /* addu v0,a0,v0 */
    }
    WR16(d + 0x6Au, v0);

    v0 = RD16(d + 0x6Au);                   /* lhu v0,0x6a(dest) */
    v0 = v0 + RD16(s + 0xB4u);              /* lhu v1,0xb4(s); addu */
    WR16(d + 0x6Au, v0);

    /* 80023808..80023820: v0 = (byte & 0x80) ? (byte & 0x7f) : 35 */
    v1 = RD8(s + 0xB9u);
    if ((v1 & 0x80u) != 0u) {
        v0 = v1 & 0x7Fu;
    } else {
        v0 = 35u;
    }
    WR16(d + 0x6Eu, v0);

    t = RD8(s + 0x13u);                     /* lbu v1,0x13(s) */
    v0 = 42u;                               /* addiu v0,zero,42; $v0 stays 42 through the next branch */
    WR8(d + 0x32u, v0);
    WR8(d + 0x13u, t);

    callee_a1 = RD32(owner + 0x5Cu);        /* lw a1,0x5c(s0) */
    if (callee_a1 != 0u) {
        /* jal 80020F20 with $a0 = owner, $a1 = callee_a1, $a2 = $a3 = dest (see the header) */
        r = m->call4(m->ctx, 0x80020F20u, owner, callee_a1, d, d);
        WR32(owner + 0x5Cu, r);
        v0 = r;
    }

done:
    m->sp_set(m->ctx, sp);
    return v0;
}
