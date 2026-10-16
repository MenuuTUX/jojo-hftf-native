#include "manual.h"
#include "hex2c_rt.h"

/* PL01X 800F4EB8..800F4FE4 (slot B). $a0 = P (struct). $a2 and $a3 are passed on to the callees without being
 * written here, so they carry whatever the previous callee left in them. $a1 is set before every call that
 * uses it, except that the call to 800268BC after 800BB30C reads it from memory. The callee leftovers are
 * read with m->exit_reg right after each call (the value the callee left, not the restored caller value).
 * a1c/a2c/a3c below are the MIPS $a1..$a3 at each jal. Branch delay slots run on both paths: sb 0x18F = 1
 * before the first branch (and sb 0x18F = 0 in the 800F77A8 path), and "addiu v0,zero,1" before the 0x207
 * compare, and "lui v0,0x8010" before the beq on v1 == 1. */
uint32_t man_PL01X_800F4EB8(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;                                    /* s0 */
    uint32_t s1, v0, v1, r;
    uint32_t a1c = a1, a2c = a2, a3c = a3;

    v0 = RD8(p + 0x201u);                               /* lbu v0,0x201(s0) */
    WR8(p + 0x18Fu, 1u);                                /* sb s1,0x18f(s0): delay slot of beq, s1 = 1, both paths */
    if (v0 != 0u) {
        /* 800F4EDC: jal 80024104, a1 = 311 */
        m->call4(m->ctx, 0x80024104u, p, 311u, a2c, a3c);
        a1c = m->exit_reg(m->ctx, 5); a2c = m->exit_reg(m->ctx, 6); a3c = m->exit_reg(m->ctx, 7);
        /* 800F4EE8: jal 800bb2a4, a1 = 10 */
        m->call4(m->ctx, 0x800BB2A4u, p, 10u, a2c, a3c);
        a1c = m->exit_reg(m->ctx, 5); a2c = m->exit_reg(m->ctx, 6); a3c = m->exit_reg(m->ctx, 7);
        /* 800F4EF0: jal 800bb30c, a0 = P; a1..a3 as the previous callee left them */
        m->call4(m->ctx, 0x800BB30Cu, p, a1c, a2c, a3c);
        a1c = m->exit_reg(m->ctx, 5); a2c = m->exit_reg(m->ctx, 6); a3c = m->exit_reg(m->ctx, 7);
        /* 800F4EF8..4F00: lui v0,0x8010; lw a1,-0x484(v0); jal 800268bc */
        a1c = RD32(0x800FFB7Cu);
        m->call4(m->ctx, 0x800268BCu, p, a1c, a2c, a3c);
        a1c = m->exit_reg(m->ctx, 5); a2c = m->exit_reg(m->ctx, 6); a3c = m->exit_reg(m->ctx, 7);
        /* 800F4F08..4F18: a1 = 2, a0 = (P[0x136] + 4) & 0xff, a2 = 9; a3 passed on */
        a1c = 2u;
        r = m->call4(m->ctx, 0x80025548u, (RD8(p + 0x136u) + 4u) & 0xFFu, a1c, 9u, a3c);
        if (r == 0u)                                    /* beq v0,zero,800f4fd4 */
            return 0u;
        WR32(r + 0x80u, p);                             /* sw s0,0x80(v0) */
        WR8(r + 0x04u, 1u);                             /* j 800f4fd4; delay slot sb s1,0x4(v0), s1 = 1 */
        return r;
    }

    /* 800F4F34: v0 == 0 */
    s1 = 0x800FF380u;                                   /* lui v1,0x8010; addiu s1,v1,-0xc80 */
    v0 = s1 + RD8(p + 0x207u);                          /* lbu v0,0x207(s0); addu v0,v0,s1 */
    a1c = RD8(v0);                                      /* lbu a1,0(v0) */
    /* 800F4F48: jal 800bb2a4, a0 = P */
    m->call4(m->ctx, 0x800BB2A4u, p, a1c, a2c, a3c);
    a1c = m->exit_reg(m->ctx, 5); a2c = m->exit_reg(m->ctx, 6); a3c = m->exit_reg(m->ctx, 7);
    /* 800F4F50: jal 800bb30c, a0 = P */
    m->call4(m->ctx, 0x800BB30Cu, p, a1c, a2c, a3c);
    a1c = m->exit_reg(m->ctx, 5); a2c = m->exit_reg(m->ctx, 6); a3c = m->exit_reg(m->ctx, 7);
    v1 = RD8(p + 0x207u);                               /* lbu v1,0x207(s0) at 800F4F58 */
    v0 = 1u;                                            /* addiu v0,zero,1 (delay slot of bne at 800F4F60) */
    if (v1 == 0u) {
        /* 800F4F68..4F70: lui v0,0x8010; lw a1,-0x474(v0); j 800f4fcc */
        a1c = RD32(0x800FFB8Cu);
    } else if (v1 == 1u) {
        /* 800F4F78 beq v1,v0 -> 800f4fc8: lui v0 (delay slot), lw a1,-0x490(v0) */
        a1c = RD32(0x800FFB70u);
    } else {
        v0 = RD32(p + 0x130u);                          /* lw v0,0x130(s0) */
        v0 = RD8(v0 + 0x20u);                           /* lbu v0,0x20(v0) */
        if (v0 != 0u) {
            /* 800F4FB8: lbu a1,1(s1); jal 800bb2a4, a0 = P */
            a1c = RD8(s1 + 1u);
            m->call4(m->ctx, 0x800BB2A4u, p, a1c, a2c, a3c);
            a1c = m->exit_reg(m->ctx, 5); a2c = m->exit_reg(m->ctx, 6); a3c = m->exit_reg(m->ctx, 7);
            /* 800F4FC4..4FC8: lui v0,0x8010; lw a1,-0x490(v0) */
            a1c = RD32(0x800FFB70u);
        } else {
            /* 800F4F98: a1 = 0, a2 = 3; jal 800f77a8 with delay slot sb zero,0x18f(s0) */
            a1c = 0u;
            a2c = 3u;
            WR8(p + 0x18Fu, 0u);
            m->call4(m->ctx, 0x800F77A8u, p, a1c, a2c, a3c);
            a1c = m->exit_reg(m->ctx, 5); a2c = m->exit_reg(m->ctx, 6); a3c = m->exit_reg(m->ctx, 7);
            /* 800F4FA8: lui v0,0x8010; lw a1,-0x470(v0); j 800f4fcc */
            a1c = RD32(0x800FFB90u);
        }
    }
    /* 800F4FCC: jal 800268bc, a0 = P; returns to the epilogue with its $v0 */
    return m->call4(m->ctx, 0x800268BCu, p, a1c, a2c, a3c);
}
