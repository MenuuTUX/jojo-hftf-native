#include "manual.h"
#include "hex2c_rt.h"

/* 8001F0A8: start of the frame's object update. The original opens a 0x20-byte frame (addiu sp,sp,-32) before the
 * SetSp() call, so SetSp() returns caller_sp - 0x20 and the saved value is that. Saves the SetSp() result (0x80035A90 with
 * $a0 = 0x1F800380) at 0x1F800000, runs 0x8002090C, clears 0x80081BCC, stores the buffer base for the current page at
 * 0x1F80005C, calls 0x8001F2E4 and 0x8002241C, walks the object list from 0x80073BC0 (indirect call through the table at
 * 0x8005A700 indexed by byte +0x1A, next object at +0x60), stores the object count to 0x80089FF0 (count = (end - base) / 40),
 * calls 0x80020C10, then calls SetSp() with the saved value and closes the frame (addiu sp,sp,32), returning the value
 * SetSp() gave back (the scratchpad $sp that the first SetSp() switched in, since the second call sees it as the old $sp).
 * The first SetSp() switches $sp to 0x1F800380 for the rest of the body: a guest call restores every register, $sp
 * included, so the C sets $sp again after that call. $a1..$a3 are passed on unchanged to every call that the original
 * makes with them unchanged, and the walk leaves $a1 at 0 (loop ran) or the entry value (no objects). */
uint32_t man_8001F0A8(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp, saved_sp, s0, idx, diff, q, result, hi, ca1;
    uint64_t prod;
    uint32_t saved_s[3];
    for (uint32_t i = 0; i < 3; ++i) saved_s[i] = m->gpr_get(m->ctx, 16u + i);

    sp = m->sp_get(m->ctx) - 0x20u;   /* 0x20 = the original "addiu sp,sp,-32" */
    m->sp_set(m->ctx, sp);

    saved_sp = m->call4pc(m->ctx, 0x80035A90u, 0x8001F0C8u, 0x1F800380u, a1, a2, a3);
    m->sp_set(m->ctx, 0x1F800380u);   /* SetSp() switched $sp to the scratchpad; the guest call restored the old $sp */
    /* Guest callbacks save these live MIPS registers to their stack. Keep the
     * original values during calls even though the C body also has local variables. */
    m->gpr_set(m->ctx, 17u, 0x1F800000u);  /* addu s1,a0,zero at 8001F0CC */
    WR32(0x1F800000u, saved_sp);
    m->call4pc(m->ctx, 0x8002090Cu, 0x8001F0D8u, 0x1F800000u, a1, a2, a3);

    WR16(0x80081BCCu, 0);
    WR32(0x1F80005Cu, 0x80081BD8u + (RD32(0x80066E14u) << 14));
    m->call4pc(m->ctx, 0x8001F2E4u, 0x8001F104u, 0x1F800000u, a1, a2, a3);
    m->call4pc(m->ctx, 0x8002241Cu, 0x8001F10Cu, 0x1F800000u, a1, a2, a3);

    ca1 = a1;
    s0 = RD32(0x80073BC0u);
    m->gpr_set(m->ctx, 16u, s0);
    if (s0 != 0u) {
        m->gpr_set(m->ctx, 18u, 0x8005A700u); /* addiu s2,v0,-22784 at 8001F120 */
        do {
            idx = RD8(s0 + 26u);
            ca1 = s0;                 /* jalr delay slot: addu a1,s0,zero */
            uint32_t target = RD32(0x8005A700u + (idx << 2));
            m->gpr_set(m->ctx, 2u, target); /* jalr target remains in v0 at callback entry */
            m->call4pc(m->ctx, target, 0x8001F144u, 0x1F800000u, ca1, a2, a3);
            s0 = RD32(s0 + 0x60u);    /* lw a1,0x60(s0): the next object, also left in $a1 */
            ca1 = s0;
            m->gpr_set(m->ctx, 16u, s0);
        } while (s0 != 0u);
    }

    diff = (0x80085BD8u + (RD32(0x80066E14u) << 14)) - RD32(0x1F80005Cu);
    prod = (uint64_t)diff * 0xCCCCCCCDu;
    hi = (uint32_t)(prod >> 32);      /* mfhi $a2 */
    q = hi >> 5;
    WR16(0x80089FF0u, q);
    m->call4pc(m->ctx, 0x80020C10u, 0x8001F194u, RD32(0x1F80005Cu), ca1, hi, a3);

    result = m->call4pc(m->ctx, 0x80035A90u, 0x8001F1A0u, RD32(0x1F800000u), ca1, hi, a3);
    m->sp_set(m->ctx, sp + 0x20u);   /* the original epilogue "addiu sp,sp,32" */
    for (uint32_t i = 0; i < 3; ++i) m->gpr_set(m->ctx, 16u + i, saved_s[i]);
    return result;
}
