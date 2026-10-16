#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800103D0(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
uint32_t man_80025E24(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);

/* Title-screen style loop (segment TEXT). The original frame is 0x50 bytes; the SPRT p lives at sp+0x18 (s3 in the MIPS).
 * Its address goes to the GPU callees, so the frame is reproduced in guest memory. Returns the $v0 left at jr ra: 0. */
uint32_t man_8001DDB8(const GuestMem *m) {
    uint32_t sp = m->sp_get(m->ctx) - 0x50u;   /* 0x50 = the original "addiu sp,sp,-80" */
    uint32_t p = sp + 0x18u;                   /* SPRT p, $s3 in the original */
    uint32_t ret25bf8, v, prod_hi, q, r, tp, clut, ent;
    uint32_t outer = 0u, s1, s2, result;
    int64_t prod;

    m->sp_set(m->ctx, sp);

    WR8(0x80066C6Eu, 1u);
    man_800103D0(m, 0u, 0u, 0u, 0u);
    m->call4(m->ctx, 0x8003609Cu, 0u, 0u, 0u, 0u);      /* DrawSync(0) */
    WR8(0x801FC09Cu, 0u);
    WR8(0x801FBC2Cu, 0u);
    m->call4(m->ctx, 0x80010914u, 1u, 0u, 0u, 0u);

    ret25bf8 = m->call4(m->ctx, 0x80025BF8u, 0u, 0u, 0u, 0u);
    v = ret25bf8 + RD8(0x80066C60u);

    /* v % 82 by the original's multiply-high sequence: mult v,0x63E7063F; mfhi; sra 5; minus sign; times 82; subtract */
    prod = (int64_t)(int32_t)v * (int64_t)(int32_t)0x63E7063Fu;
    prod_hi = (uint32_t)(uint64_t)((uint64_t)prod >> 32);
    q = (uint32_t)((int32_t)prod_hi >> 5) - (uint32_t)((int32_t)v >> 31);
    r = v - q * 82u;

    m->call4(m->ctx, 0x8001ED60u, RD16(0x80059CACu + 2u * r), 0u, 0u, 0u);
    man_80025E24(m, 0x80119800u, 0u, 0u, 0u);

    do {
        m->call4(m->ctx, 0x8003C210u, 0u, 0u, 0u, 0u);  /* VSync(0) */
        s1 = 0u;
        s2 = 0u;
        do {
            ent = 0x80059D50u + 12u * s1;
            tp = m->call4(m->ctx, 0x800397F0u, 1u, 0u,
                          (uint32_t)RDS16(ent + 8u), (uint32_t)RDS16(ent + 10u));
            m->call4(m->ctx, 0x80039980u, p, 0u, 1u, (uint32_t)(uint16_t)tp);
            m->call4(m->ctx, 0x80036634u, p, 0u, 0u, 0u);
            m->call4(m->ctx, 0x80039920u, p, 0u, 0u, 0u);
            clut = m->call4(m->ctx, 0x80039830u, 0u, 480u, 0u, 0u);
            WR16(p + 0x0Eu, clut);
            WR8(p + 0x04u, 0x80u);
            WR8(p + 0x05u, 0x80u);
            WR8(p + 0x06u, 0x80u);
            WR8(p + 0x0Cu, 0u);
            WR8(p + 0x0Du, 0u);
            WR16(p + 0x08u, RD16(ent + 0u));
            WR16(p + 0x0Au, RD16(ent + 2u));
            WR16(p + 0x10u, RD16(ent + 4u));
            WR16(p + 0x12u, RD16(ent + 6u));
            m->call4(m->ctx, 0x80036634u, p, 0u, 0u, 0u);
            s2 += 1u;
            s1 = (s1 & 3u) + 1u;
        } while (s2 < 4u);
        outer += 1u;
        result = (outer < 2u) ? 1u : 0u;
    } while (result != 0u);

    m->sp_set(m->ctx, sp + 0x50u);
    return result;
}
