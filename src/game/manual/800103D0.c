#include "manual.h"
#include "hex2c_rt.h"

/* 800103D0 (TEXT). Inputs $a0..$a3 are all read (saved at entry). Returns 0x80060000 ($v0 from the last lui).
 * Leaf callees 8003A3C0 (SetGeomOffset: shifts $a0 and $a1 left by 16, no other GPR writes) and 8003A3E0 (GTE only)
 * leave $a1 = (($a3 / 2) << 16) for the second call. Other callees: $a1..$a3 not set by the original are passed as 0,
 * except where a leaf callee provably leaves the register unchanged (tracked in a2_join / a3_join). */
uint32_t man_800103D0(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t a2_join;
    uint32_t a3_join;

    m->call4(m->ctx, 0x8003A3C0u, (uint32_t)((int32_t)a2 / 2), (uint32_t)((int32_t)a3 / 2), a2, a3);
    m->call4(m->ctx, 0x8003A3E0u, 544u, ((uint32_t)((int32_t)a3 / 2)) << 16, a2, a3);

    if (RD8(0x80066C6Eu) != 0u) {
        m->call5(m->ctx, 0x800396F0u, 0x801FBC14u, 0u, 0u, 384u, 480u);
        m->call5(m->ctx, 0x800397B0u, 0x801FBC00u, 0u, 0u, 384u, 480u);
        m->call5(m->ctx, 0x800396F0u, 0x801FC084u, 0u, 0u, 384u, 480u);
        m->call5(m->ctx, 0x800397B0u, 0x801FC070u, 0u, 0u, 384u, 480u);
        WR8(0x801FC080u, 1u);
        WR8(0x801FBC10u, 1u);
        a2_join = 0u;
        a3_join = 384u;
    } else {
        m->call5(m->ctx, 0x800396F0u, 0x801FBC14u, 0u, 8u, a2, a3);
        m->call5(m->ctx, 0x800397B0u, 0x801FBC00u, 0u, 240u, a0, a1);
        m->call5(m->ctx, 0x800396F0u, 0x801FC084u, 0u, 248u, a2, a3);
        m->call5(m->ctx, 0x800397B0u, 0x801FC070u, 0u, 0u, a0, a1);
        WR8(0x801FC080u, 0u);
        WR8(0x801FBC10u, 0u);
        a2_join = 0u;
        a3_join = a0;
    }

    /* join: $a0/$a1 set here; $a2/$a3 are what the last 800397B0 (leaf, no $a2/$a3 writes) left */
    m->call4(m->ctx, 0x80010628u,
             (uint32_t)(int32_t)RDS16(0x8006CE5Eu),
             (uint32_t)(int32_t)(int16_t)(uint16_t)(0u - (uint32_t)RD16(0x8006CE60u)),
             a2_join, a3_join);

    WR8(0x801FC09Cu, 1u);
    WR8(0x801FBC2Cu, 1u);
    WR8(0x801FC09Au, 1u);
    WR8(0x801FBC2Au, 1u);
    WR8(0x801FC09Bu, 0u);
    WR8(0x801FBC2Bu, 0u);
    WR8(0x801FBC2Du, 0u);
    WR8(0x801FBC2Eu, 0u);
    WR8(0x801FBC2Fu, 0u);
    WR8(0x801FC09Du, 0u);
    WR8(0x801FC09Eu, 0u);
    WR8(0x801FC09Fu, 0u);

    m->call4(m->ctx, 0x800362B0u, 0x801FBC00u, 0u, 0u, 0u);
    m->call4(m->ctx, 0x800362B0u, 0x801FC070u, 0u, 0u, 0u);
    m->call4(m->ctx, 0x80036588u, 0x801FB800u, 256u, 0u, 0u);
    m->call4(m->ctx, 0x80036588u, 0x801FBC70u, 256u, 0u, 0u);

    WR32(0x80066E14u, 0u);
    WR32(0x80066E10u, 0x801FB800u);
    return 0x80060000u;
}
