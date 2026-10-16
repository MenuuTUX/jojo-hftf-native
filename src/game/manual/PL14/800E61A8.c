#include "manual.h"
#include "hex2c_rt.h"

/* PL14 800E61A8..800E63E8 (slot A, base 800DF000). $a0 = P (kept in $s0). $s1 = Q = word P+0x80, read before the first branch.
 * $a1, $a2, $a3 go to each callee as the previous step left them: the entry values until the first jal, then what the
 * callee left (gpr_get after call4). Each jal's $a0 is set in its delay slot (P, or Q for jal 80029714).
 * Every path exits at 800e63e4 (jr ra). $v0 at the exit is the value the last instruction on that path left in it. The
 * delay slots that set $v0 (addiu v0,zero,1 at 800e61c8, 800e62d0, 800e631c, the lui and addiu at 800e61d0 paths) run on
 * the branch paths too, and are handled in the order the MIPS runs them. */

static void reload_args(const GuestMem *m, uint32_t *a1, uint32_t *a2, uint32_t *a3) {
    *a1 = m->gpr_get(m->ctx, 5u);
    *a2 = m->gpr_get(m->ctx, 6u);
    *a3 = m->gpr_get(m->ctx, 7u);
}

/* 800e63d0: lbu v0,5(a0); addiu a1,zero,2; addiu v0,v0,1; jal 80025a50; sb v0,5(a0) in the delay slot. */
static uint32_t tail_63d0(const GuestMem *m, uint32_t p, uint32_t a2, uint32_t a3) {
    WR8(p + 5u, RD8(p + 5u) + 1u);
    return m->call4(m->ctx, 0x80025A50u, p, 2u, a2, a3);
}

/* 800e62b0 path: halfword +0x6E at or above 36. Every sub-path exits through jr ra. */
static uint32_t path_62b0(const GuestMem *m, uint32_t p, uint32_t q, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint8_t flag = RD8(0x80066CA2u);                    /* lui v0,0x8006; lbu v0,0x6ca2(v0) */
    uint32_t v1, v0, f0old, ee_new, hu;
    int32_t hs;

    if (flag != 0u)
        return flag;                                    /* bne v0,zero,800e63e4 (delay nop) */

    v1 = RD8(p + 7u);                                   /* lbu v1,7(s0) (800e62c4) */
    if (v1 == 0u) {
        /* 800e62d4 (its delay slot addiu v0,zero,1 runs before this) */
        ee_new = (RD16(p + 0xEEu) + 16u) & 0xFFFFu;     /* lhu v0,0xee; addiu v0,v0,16; sh v0,0xee */
        WR16(p + 0xEEu, ee_new);
        f0old = RD16(p + 0xF0u);                        /* lhu v0,0xf0 */
        v0 = f0old + 16u;                               /* addiu v0,v0,16 */
        WR16(p + 0xF0u, v0);                            /* sh v0,0xf0: delay slot of bne at 800e62f4, both paths */
        if (RD16(p + 0xEEu) < 63u)                      /* lhu v1,0xee; sltiu v1,v1,63; bne v1,zero,800e63e4 */
            return v0;
        WR16(p + 0xCu, 4u);                             /* addiu v0,zero,4; sh v0,0xc */
        v0 = RD8(p + 7u);                               /* lbu v0,7(s0) */
        WR16(p + 0xF0u, 63u);                           /* addiu v1,zero,63; sh v1,0xf0 */
        WR16(p + 0xEEu, 63u);                           /* sh v1,0xee (delay slot of j 800e6374) */
        v0 = v0 + 1u;                                   /* 800e6374: addiu v0,v0,1 */
        WR8(p + 7u, v0);                                /* sb v0,7(s0) */
        return v0;
    }

    if (v1 == 1u) {                                     /* bne v1,v0 (v0 = 1) not taken */
        /* 800e6320: halfword +0x0C decremented; the signed result is compared with -1 */
        hs = (int16_t)(uint16_t)(RD16(p + 0xCu) - 1u);  /* lhu v0,0xc; addiu v0,v0,-1; sh v0,0xc; sll/sra 16 */
        WR16(p + 0xCu, (uint32_t)hs);
        if (hs != -1)
            return (uint32_t)hs;                        /* bne v0,v1,800e63e4 (v1 = -1 from the delay slot) */
        m->call4(m->ctx, 0x80029714u, q, 13u, 2u, a3); /* addu a0,s1,zero; jal 80029714 with a1 = 13, a2 = 2 (delay) */
        reload_args(m, &a1, &a2, &a3);
        if ((RD8(p + 4u) & 1u) != 0u) {                 /* lbu v0,4(s0); andi v0,v0,1; beq v0,zero,800e6368 */
            m->call4(m->ctx, 0x80026AFCu, p, a1, a2, a3); /* jal 80026afc, delay addu a0,s0,zero */
            reload_args(m, &a1, &a2, &a3);
        }
        v0 = RD8(p + 7u);                               /* 800e6368: lbu v0,7(s0) */
        WR16(p + 0xCu, 2u);                             /* addiu v1,zero,2; sh v1,0xc */
        v0 = v0 + 1u;                                   /* 800e6374 */
        WR8(p + 7u, v0);                                /* sb v0,7(s0) */
        return v0;
    }

    /* 800e6380: v1 (the byte at +7) is neither 0 nor 1 */
    hs = (int16_t)RD16(p + 0xCu);                       /* lh v0,0xc(s0) */
    hu = RD16(p + 0xCu);                                /* lhu v1,0xc(s0) */
    if (hs == 0) {                                      /* beq v0,zero,800e6398 (delay addiu v0,v1,-1 runs first) */
        m->call4(m->ctx, 0x80026A04u, p, a1, a2, a3);  /* jal 80026a04, delay addu a0,s0 */
        reload_args(m, &a1, &a2, &a3);
    } else {
        WR16(p + 0xCu, hu - 1u);                        /* j 800e63a0, delay sh v0,0xc (v0 = v1 - 1) */
    }
    return m->call4(m->ctx, 0x800B428Cu, p, a1, a2, a3); /* jal 800b428c, delay addu a0,s0 */
}

uint32_t man_PL14_800E61A8(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;
    uint32_t q, r, tbl;
    int32_t s;
    uint8_t b6 = RD8(p + 6u);                           /* lbu v1,6(s0) (800e61bc) */
    q = RD32(p + 0x80u);                                /* lw s1,0x80(s0) (800e61c0) */

    if (b6 != 0u) {                                     /* bne v1,zero,800e63b0 (taken) */
        if (b6 != 1u)                                   /* 800e63b0: bne v1,v0 with v0 = 1 (delay of 800e61c4) */
            return 1u;                                  /* exit with $v0 = 1 */
        m->call4(m->ctx, 0x80026A04u, p, a1, a2, a3);  /* 800e63b8 jal 80026a04, $a0 = s0 */
        reload_args(m, &a1, &a2, &a3);
        /* 800e63c0: lb v1,0x4d(s0); addiu v0,zero,-1; bne v1,v0,800e63e4 (delay addu a0,s0) */
        if ((int8_t)RD8(p + 0x4Du) != -1)
            return 0xFFFFFFFFu;
        return tail_63d0(m, p, a2, a3);
    }

    /* 800e61cc: jal 800e64c4, $a0 = s0 (delay nop), $a1..$a3 as the entry left them */
    r = m->call4(m->ctx, 0x800E64C4u, p, a1, a2, a3);
    reload_args(m, &a1, &a2, &a3);
    if (r != 0u)                                        /* bne v0,zero,800e63d0 */
        return tail_63d0(m, p, a2, a3);

    /* 800e61dc: word Q+4 masked with 0x00FFFF00 must equal 0x100 */
    if ((RD32(q + 4u) & 0x00FFFF00u) != 0x100u)
        return tail_63d0(m, p, a2, a3);

    if (RD8(p + 0x139u) != 0u) {                        /* lbu v0,0x139(s0); beq v0,zero,800e6240 (not taken) */
        uint32_t v;
        WR8(p + 6u, RD8(p + 6u) + 1u);                  /* lbu v0,6(s0); addiu; sb */
        v = RD8(RD32(p + 0x108u) + 0x32u);              /* lw v1,0x108(s0); lbu v1,0x32(v1) */
        WR16(p + 0xCu, 8u);                             /* addiu v0,zero,8; sh v0,0xc */
        WR8(p + 0xF8u, 0u);                             /* sb zero,0xf8 */
        WR16(p + 0xF0u, 95u);                           /* sh v0(95),0xf0 */
        WR16(p + 0xEEu, 95u);                           /* sh v0(95),0xee */
        WR8(p + 0x32u, v - 1u);                         /* addiu v1,v1,-1; sb v1,0x32 (delay of j 800e627c) */
    } else {
        s = (int16_t)RD16(p + 0x6Eu);                   /* lh v0,0x6e(s0) */
        /* slti v0,v0,36; beq v0,zero,800e62b0 (delay addiu v0,zero,8 runs on both paths) */
        if (s >= 36)
            return path_62b0(m, p, q, a1, a2, a3);
        WR16(p + 0xCu, 8u);                             /* sh v0,0xc (v0 = 8) */
        WR16(p + 0x6Eu, 35u);                           /* addiu v0,zero,35; sh v0,0x6e */
        WR8(p + 0xF8u, 0u);                             /* sb zero,0xf8 */
        WR16(p + 0xF0u, 95u);                           /* sh v1,0xf0 (v3 = 95) */
        WR16(p + 0xEEu, 95u);                           /* sh v1,0xee */
        WR8(p + 6u, RD8(p + 6u) + 1u);                  /* lbu v0,6; addiu v0,v0,1; sb v0,6 */
    }

    /* 800e627c: lb v1,0x136(s1); P+0x1E = 24 * v1 + 223; jal 800268bc with $a1 = word 0x800E8E34 */
    s = (int8_t)RD8(q + 0x136u);
    WR16(p + 0x1Eu, (uint32_t)(s * 24 + 223));
    tbl = RD32(0x800E8E34u);                            /* lui v0,0x800f; lw a1,-0x71cc(v0) */
    return m->call4(m->ctx, 0x800268BCu, p, tbl, a2, a3); /* jal 800268bc, delay addu a0,s0 */
}
