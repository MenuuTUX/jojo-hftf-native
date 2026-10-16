#include "manual.h"
#include "hex2c_rt.h"

/* Thread dispatcher (called once per frame by main). Walks the three thread records at 0x801FC400 (stride 0x80) with the current
 * record pointer in 0x80066294: a record in state 3 is reopened (OpenTh(entry +0xC, stack +0x8, gp +0x10) inside a critical
 * section, handle to +0x4); a record in state 2 or 3 is set to 4 (running) and entered with ChangeTh(handle); after each switch
 * returns, the vblank callback is set again to 0x80010BD4. Returns the final record pointer (0x801FC580), as $v0 holds it. */
uint32_t man_800107A0(const GuestMem *m) {
    uint32_t p, st, h, entry_sp, frame_sp, entry_s0, entry_ra;
    entry_sp = m->sp_get(m->ctx);
    entry_s0 = m->gpr_get(m->ctx, 16u);
    entry_ra = m->gpr_get(m->ctx, 31u);
    frame_sp = entry_sp - 24u;
    m->sp_set(m->ctx, frame_sp);
    WR32(frame_sp + 16u, entry_s0);
    WR32(frame_sp + 20u, entry_ra);
    m->gpr_set(m->ctx, 16u, 0x80060000u);
    WR32(0x80066294u, 0x801FC400u);
    do {
        p = RD32(0x80066294u);
        st = RD16(p);
        if (st == 3u) {
            m->call4(m->ctx, 0x80035A70u, 0, 0, 0, 0);                                     /* EnterCriticalSection */
            p = RD32(0x80066294u);
            h = m->call4(m->ctx, 0x80035A40u, RD32(p + 0xCu), RD32(p + 0x8u), RD32(p + 0x10u), 0);   /* OpenTh */
            WR32(RD32(0x80066294u) + 4u, h);
            m->call4(m->ctx, 0x80035A80u, 0, 0, 0, 0);                                     /* ExitCriticalSection */
            st = 2u;
        }
        if (st == 2u) {
            p = RD32(0x80066294u);
            WR16(p, 4u);
            m->gpr_set(m->ctx, 2u, p);                                                     /* lw v0,0x6294(s0) */
            m->gpr_set(m->ctx, 3u, 4u);                                                    /* addiu v1,zero,4 */
            m->call4pc(m->ctx, 0x80035A60u, 0x80010828u, RD32(p + 4u),
                       m->gpr_get(m->ctx, 5u), m->gpr_get(m->ctx, 6u), m->gpr_get(m->ctx, 7u));
                                                                                           /* jal ChangeTh; save the real MIPS continuation */
            m->gpr_set(m->ctx, 4u, 0x80010BD4u);                                           /* lui/addiu a0 in the continuation */
            m->call4(m->ctx, 0x8003C530u, 0x80010BD4u, 0, 0, 0);                           /* VSyncCallback(0x80010BD4) */
        }
        p = RD32(0x80066294u) + 128u;
        WR32(0x80066294u, p);
    } while (p <= 0x801FC57Fu);
    m->gpr_set(m->ctx, 2u, p);
    m->gpr_set(m->ctx, 3u, p > 0x801FC57Fu);
    m->gpr_set(m->ctx, 16u, entry_s0);
    m->gpr_set(m->ctx, 31u, entry_ra);
    m->sp_set(m->ctx, entry_sp);
    return p;
}
