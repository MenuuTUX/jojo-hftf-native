#include "manual.h"
#include "hex2c_rt.h"

/* 800A2148 (GAMESS). The real body runs 800A2148..800A2274 (jr ra at 800A2274); the index size of 76 stops at the IDA split 800A2194.
 * $a0 = list index (masked to a byte). s0 = node at 0x80073B40[idx]; each node's next pointer is at +0x5C.
 * Each node's handler address comes from the tables at s2/s3/s4/s5 (see below) and is called through $v0 with $a0 = node.
 * $a1..$a3 are not read by the code; the handler call passes 0 for them (house convention, as in 800FD244). The original passes
 * the entry values (and later the previous handler's leftovers), so this is a doubt: GUESS handlers ignore $a1..$a3.
 * $v0 at return: 0x800F0000 for an empty list (lui in the delay slot at 800A2188); 7 when the last node has byte 0 == 0
 * (addiu in the delay slot at 800A21C0); otherwise 0 or the last handler's $v0. */
uint32_t man_800A2148(const GuestMem *m, uint32_t a0) {
    uint32_t s1 = a0 & 0xFFu;
    uint32_t s0 = RD32(0x80073B40u + s1 * 4u);
    uint32_t s2;
    uint32_t s3;
    uint32_t s4;
    uint32_t s5;
    uint32_t v0;
    uint32_t v1;
    uint32_t addr;

    if (s0 == 0u)
        return 0x800F0000u;
    s5 = 0x800F0000u - 7920u;
    s4 = 0x800F0000u - 28132u;
    s3 = 0x80060000u - 21876u;
    s2 = (s1 << 1) + (0x80060000u - 21940u);

loop:
    v0 = RD8(s0);
    if (v0 == 0u) {
        v0 = 7u;
        goto next;
    }
    v0 = 7u;
    if (s1 != 7u)
        goto generic;
    v0 = RD8(s0 + 2u) & 0x80u;
    if (v0 == 0u)
        goto generic;
    v0 = 0x80060000u;
    v1 = RD16(v0 + 0x6C48u);
    v0 = 4u;
    if (v1 != 4u)
        goto alt;
    addr = ((RD8(s0 + 2u) - 128u) << 2) + s5;
    goto load;

alt:
    addr = ((RD8(s0 + 2u) - 128u) << 2) + s4;
    goto load;

generic:
    v0 = RD16(s2);
    v1 = RD8(s0 + 2u);
    v0 = v0 + v1;
    v0 = v0 << 2;
    addr = v0 + s3;

load:
    v0 = RD32(addr);
    v0 = m->calli(m->ctx, v0, s0, 0u, 0u, 0u);

next:
    s0 = RD32(s0 + 0x5Cu);
    if (s0 != 0u)
        goto loop;
    return v0;
}
