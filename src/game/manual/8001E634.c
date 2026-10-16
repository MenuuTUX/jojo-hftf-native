#include "manual.h"
#include "hex2c_rt.h"

/* 8001E634 (TEXT). a0 = index byte: table = 0x8005A194 + 2 * (a0 & 0xFF) (halfwords).
 * 8001D984(0x800DF000, 760) first: 0x800DF000 is lui 0x800D + ori 0xF000 (not a memory read of jpt_800E052C).
 * Then do { 80018034(0x800E2000, lh table[index]); while (byte 0x80079247 == 0) 80010914(1); r = byte 0x80079250; }
 * while (r != 0). The constant 0x800E2000 is lui 0x800E + ori 0x2000. $a2/$a3 are never written and pass on
 * unchanged to 8001D984 and 80018034 (nargs 4); 80010914 gets 0 in $a1 (the original's $a1 there is the residue of
 * 80018034, and 80010914 reads only $a0). Returns the last lbu of 0x80079250, which is 0 at exit. */
uint32_t man_8001E634(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t s2;
    uint32_t r;

    m->call4(m->ctx, 0x8001D984u, 0x800DF000u, 760u, a2, a3);
    s2 = ((a0 & 0xFFu) << 1) + 0x8005A194u;
    do {
        m->call4(m->ctx, 0x80018034u, 0x800E2000u, (uint32_t)(int32_t)RDS16(s2), a2, a3);
        while (RD8(0x80079247u) == 0) {
            m->call4(m->ctx, 0x80010914u, 1u, 0u, a2, a3);
        }
        r = RD8(0x80079250u);
    } while (r != 0);
    return r;
}
