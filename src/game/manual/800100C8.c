#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800107A0(const GuestMem *m);

/* main: start-up, then the frame loop. Per frame: reset the vblank tick 0x800739B2 and root counter 1 (ResetRCnt(0xF2000001));
 * unless the 0x80073CC2 countdown is above 0x4000: thread dispatcher, mode machine 0x80017D20, text 0x80011730, frame counter
 * 0x80066C61 + 1; DrawSync(0); 0x80010DD4; wait for the vblank callback to count at least one vblank; unless the countdown is
 * above 0x4000: swap the two display buffers (index 0x80066E14, current one 0x80066E10 = 0x801FB800 + index * 0x470), PutDispEnv,
 * PutDrawEnv, 0x80010EDC and DrawOTag on the old ordering table, ClearOTagR(new, 256), thread wait countdown 0x80010D6C; then the
 * 0x80073CC2 countdown step and the soft-reset check (both pads holding the 0x0900 combination for 120 frames calls 0x80010650).
 * Never returns. Runs with $sp lowered by the original's 48-byte frame. */
uint32_t man_800100C8(const GuestMem *m) {
    uint32_t v, old, idx, ot;
    m->sp_set(m->ctx, m->sp_get(m->ctx) - 48u);
    m->call4(m->ctx, 0x80010004u, 0, 0, 0, 0);                 /* __main */
    m->call4(m->ctx, 0x8003C4A0u, 0, 0, 0, 0);                 /* ResetCallback */
    m->call4(m->ctx, 0x8003C594u, 0, 0, 0, 0);                 /* StopCallback */
    m->call4(m->ctx, 0x800250F8u, 3, 0, 0, 0);
    m->call4(m->ctx, 0x800106DCu, 0, 0, 0, 0);
    m->call4(m->ctx, 0x80010354u, 0, 0, 0, 0);
    m->call4(m->ctx, 0x800103D0u, 384, 240, 384, 224);
    m->call4(m->ctx, 0x80010754u, 0, 0, 0, 0);
    m->call4(m->ctx, 0x800108B8u, 0, 0x80010A90u, 0, 0);       /* create slot 0 with the boot thread */
    m->call4(m->ctx, 0x800113F0u, 0x8006D1F0u, 0, 0, 0);
    for (;;) {
        WR16(0x800739B2u, 0);
        m->call4(m->ctx, 0x80035BD8u, 0xF2000001u, 0, 0, 0);   /* ResetRCnt(RCntCNT1) */
        if (RD16(0x80073CC2u) < 0x4001u) {
            man_800107A0(m);
            m->call4(m->ctx, 0x80017D20u, 0, 0, 0, 0);
            m->call4(m->ctx, 0x80011730u, RD32(0x80066E10u) + 8u, 0x8006D1F0u, 0, 0);
            WR8(0x80066C61u, RD8(0x80066C61u) + 1u);
        }
        m->call4(m->ctx, 0x8003609Cu, 0, 0, 0, 0);             /* DrawSync(0) */
        m->call4(m->ctx, 0x80010DD4u, 0, 0, 0, 0);
        while (RD16(0x800739B2u) == 0u)
            m->call4(m->ctx, 0x8003C210u, 0, 0, 0, 0);         /* VSync(0) */
        if (RD16(0x80073CC2u) < 0x4001u) {
            idx = RD32(0x80066E14u) ^ 1u;
            old = RD32(0x80066E10u);
            ot = 0x801FB800u + idx * 1136u;
            WR32(0x80066E14u, idx);
            WR32(0x80066E10u, ot);
            m->call4(m->ctx, 0x800368CCu, ot + 1024u, 0, 0, 0);               /* PutDispEnv */
            m->call4(m->ctx, 0x80036700u, RD32(0x80066E10u) + 1044u, 0, 0, 0); /* PutDrawEnv */
            m->call4(m->ctx, 0x80010EDCu, old + 1020u, 0, 0, 0);
            m->call4(m->ctx, 0x80036690u, old + 1020u, 0, 0, 0);             /* DrawOTag */
            m->call4(m->ctx, 0x80036588u, RD32(0x80066E10u), 256, 0, 0);      /* ClearOTagR */
            m->call4(m->ctx, 0x80010D6Cu, 0, 0, 0, 0);
        }
        v = RD16(0x80073CC2u);
        WR16(0x80073CC2u, (v & 0x1Fu) ? v - 1u : (v | 0x10u));
        if (RD8(0x80079244u) == 1u || RD32(0x80055E5Cu) != 0u) {
            WR32(0x80055E64u, 0);
            WR32(0x80055E68u, 0);
            continue;
        }
        WR32(0x80055E64u, (RD16(0x8006CEBAu) & 0x900u) == 0x900u ? RD32(0x80055E64u) + 1u : 0u);
        WR32(0x80055E68u, (RD16(0x8006CECAu) & 0x900u) == 0x900u ? RD32(0x80055E68u) + 1u : 0u);
        if ((int32_t)RD32(0x80055E64u) >= 120 || (int32_t)RD32(0x80055E68u) >= 120)
            m->call4(m->ctx, 0x80010650u, 0, 0, 0, 0);
    }
}
