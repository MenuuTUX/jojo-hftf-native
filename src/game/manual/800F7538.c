#include "manual.h"
#include "hex2c_rt.h"

/* 800F7538 (PL02X, 300 bytes): sub_800F7538 in ida/decomp/fight/PL02X/800f7538.c, followed path by path through the MIPS.
 * $a0 is the object (s0). The $a1..$a3 values below are the ones the MIPS holds at each jal: a constant or a load the caller
 * sets in its own delay slot, or the value left by the previous jal in this function, or the incoming register.
 *   block0 (byte 0x207 == 0): 800FB7E8(s0, 0, 5, a3); 800BB2A4(s0, 22, 5, a3); 800268BC(s0, [0x801022C4], 5, a3)
 *   block1 (byte == 1):       800FB7E8(s0, 0, 0, a3); 800BB2A4(s0, 21, 0, a3); 800268BC(s0, [0x801022C8], 0, a3)
 *   block2 (byte == 2):       800FB7E8(s0, 0, 3, a3); 800BB2A4(s0, 20, 3, a3); 800268BC(s0, [0x801022BC], 3, a3)
 *   block3 (byte == 3):       800BB2A4(s0, 23, a2, a3); 800268BC(s0, [0x801022C0], a2, a3)
 * The final 800BB30C gets $a1 = 0 on the block0 and block1 paths (the delay slot at 800F75F8 runs on every path), the last
 * value passed to 800268BC on the block2 and block3 paths, and $a2 = 5, 0, 3 or the incoming $a2 for block0, block1,
 * block2 or block3 (the last value this function set). $a3 is never written by this function, so it passes on unchanged.
 * Not modelled: a callee's own writes to $a1..$a3 after it returns (the harness keeps the caller's registers across
 * call4). 800FB7E8 writes $a2 and $a3 (through 80025548 and 800258F0), and 800268BC calls through a table with jalr,
 * so the register values after those callees are not measured.
 * Returns the $v0 of 800BB30C. The frame is 24 bytes, as in the original. */
uint32_t man_800F7538(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t s0 = a0;
    uint32_t r1 = a1;           /* $a1 as the MIPS holds it at the next jal */
    uint32_t r2 = a2;           /* $a2 */
    uint32_t r3 = a3;           /* $a3, never written by this function */
    uint32_t sp, v, t, result;
    uint16_t h;

    sp = m->sp_get(m->ctx);
    m->sp_set(m->ctx, sp - 24u);

    v = RD8(s0 + 0x207u);               /* lbu v0,0x207(s0) */
    t = RD32(s0 + 0x170u);              /* lw v1,0x170(s0) */
    WR8(s0 + 0xB8u, 0u);                /* sb zero,0xb8(s0): delay slot of the bne, runs on both paths */

    if (v == 0u) {
        /* 800F7558..800F7580: halfword at +0x6A is moved by +32 when byte [t+0xB] is nonzero, else -32 */
        h = RD16(s0 + 0x6Au);
        if (RD8(t + 0xBu) != 0u) {
            h = (uint16_t)(h + 32u);
        } else {
            h = (uint16_t)(h - 32u);
        }
        WR16(s0 + 0x6Au, h);

        /* 800F7584..800F7590: 800FB7E8(s0, 0, 5) */
        r1 = 0u;
        r2 = 5u;
        m->call4(m->ctx, 0x800FB7E8u, s0, r1, r2, r3);

        /* 800F7594..800F759C: 800BB2A4(s0, 22); $a2 is still 5 */
        r1 = 22u;
        m->call4(m->ctx, 0x800BB2A4u, s0, r1, r2, r3);

        /* 800F75A0..800F75AC: 800268BC(s0, word at 0x801022C4) */
        r1 = RD32(0x801022C4u);
        m->call4(m->ctx, 0x800268BCu, s0, r1, r2, r3);
    }

    v = RD8(s0 + 0x207u);               /* 800F75B0 */
    if (v == 1u) {
        /* 800F75C0..800F75CC: 800FB7E8(s0, 0, a2 = a1 = 0) */
        r1 = 0u;
        r2 = 0u;
        m->call4(m->ctx, 0x800FB7E8u, s0, r1, r2, r3);

        /* 800F75D0..800F75D8: 800BB2A4(s0, 21); $a2 is still 0 */
        r1 = 21u;
        m->call4(m->ctx, 0x800BB2A4u, s0, r1, r2, r3);

        /* 800F75DC..800F75E8: 800268BC(s0, word at 0x801022C8) */
        r1 = RD32(0x801022C8u);
        m->call4(m->ctx, 0x800268BCu, s0, r1, r2, r3);

        v = RD8(s0 + 0x207u);           /* 800F75EC */
    }

    /* 800F75F4: bne v,2 skips block2; its delay slot 800F75F8 sets $a1 = 0 on every path */
    r1 = 0u;
    if (v == 2u) {
        /* 800F75FC..800F7604: 800FB7E8(s0, 0, 3) */
        r2 = 3u;
        m->call4(m->ctx, 0x800FB7E8u, s0, r1, r2, r3);

        /* 800F7608..800F7610: 800BB2A4(s0, 20); $a2 is still 3 */
        r1 = 20u;
        m->call4(m->ctx, 0x800BB2A4u, s0, r1, r2, r3);

        /* 800F7614..800F7620: 800268BC(s0, word at 0x801022BC) */
        r1 = RD32(0x801022BCu);
        m->call4(m->ctx, 0x800268BCu, s0, r1, r2, r3);
    }

    v = RD8(s0 + 0x207u);               /* 800F7624 */
    if (v == 3u) {
        /* 800F7634..800F7638: 800BB2A4(s0, 23); $a2 and $a3 are the values they had on entry to this block */
        r1 = 23u;
        m->call4(m->ctx, 0x800BB2A4u, s0, r1, r2, r3);

        /* 800F763C..800F7644: 800268BC(s0, word at 0x801022C0) */
        r1 = RD32(0x801022C0u);
        m->call4(m->ctx, 0x800268BCu, s0, r1, r2, r3);
    }

    /* 800F764C: 800BB30C(s0) with $a1, $a2, $a3 as tracked above; returns its $v0 */
    result = m->call4(m->ctx, 0x800BB30Cu, s0, r1, r2, r3);
    m->sp_set(m->ctx, sp);
    return result;
}
