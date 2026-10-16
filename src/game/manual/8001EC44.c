#include "manual.h"
#include "hex2c_rt.h"

/* 8001EC44 (TEXT). Selects a table by the signed byte at a0+0x136:
 *   0: base 0x800DF000 (lui 0x800D + ori 0xF000), table 0x8005A298 (halfwords, index = 2 * signed byte at a0+0x137)
 *   nonzero: base 0x800F4800 (lui 0x800F + ori 0x4800), table 0x8005A2CC
 * do { 80018034(base, lh table[index]); while (byte 0x80079247 == 0) 80010914(1); r = byte 0x80079250; }
 * while (r != 0). The index byte at a0+0x137 is re-read on every pass. $a2/$a3 are never written and pass on
 * unchanged to both callees (nargs 4); 80010914 gets 0 in $a1 (it reads only $a0). Returns the last lbu of
 * 0x80079250, which is 0 at exit. */
uint32_t man_8001EC44(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t s2 = a0;
    uint32_t base;
    uint32_t table;
    uint32_t r;

    if (RDS8(s2 + 0x136u) == 0) {
        base = 0x800DF000u;
        table = 0x8005A298u;
    } else {
        base = 0x800F4800u;
        table = 0x8005A2CCu;
    }
    do {
        uint32_t idx = ((uint32_t)(int32_t)RDS8(s2 + 0x137u)) << 1;
        m->call4(m->ctx, 0x80018034u, base, (uint32_t)(int32_t)RDS16(table + idx), a2, a3);
        while (RD8(0x80079247u) == 0) {
            m->call4(m->ctx, 0x80010914u, 1u, 0u, a2, a3);
        }
        r = RD8(0x80079250u);
    } while (r != 0);
    return r;
}
