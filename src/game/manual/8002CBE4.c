#include "manual.h"
#include "hex2c_rt.h"

/* 8002CBE4: four passes over the 256-byte records at 0x80066298 + 256*k. The selector pointer is s0 = record+0xEC
 * (the address 0x80066384 + 256*k, not the word stored there); a record runs its handler when its byte +0x00 and its
 * byte +0x01 are both nonzero. The handler is table[selector byte] at 0x8005B014, called with $a0 = record and the
 * entry $a1..$a3 passed on unchanged (the handler gets the caller's $a1..$a3, not garbage). After the loop it writes
 * entry 0x80095998 + (RD32(0x80066E14) << 4): byte +3 = 1, word +4 = 0xE1000200, and links it into the word at
 * 0x80066E10+12 (keeps that word's top byte, takes the low 24 bits of the entry address). $a0 is written before any read. */
uint32_t man_8002CBE4(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx) - 0x28u;   /* 0x28 = the original "addiu sp,sp,-40" */
    uint32_t s1, s0, n, v3, v4, res;
    (void)a0;
    m->sp_set(m->ctx, sp);

    s1 = 0x80066298u;
    s0 = s1 + 0xECu;
    n = 4u;
    do {
        if (RD8(s1) != 0u && RD8(s0 - 0xEBu) != 0u) {
            (void)m->calli(m->ctx, RD32(0x8005B014u + (uint32_t)RD8(s0) * 4u), s1, a1, a2, a3);
        }
        s0 += 256u;
        n--;
        s1 += 256u;
    } while (n != 0u);

    v3 = 0x80095998u + (RD32(0x80066E14u) << 4);
    WR8(v3 + 3u, 1u);
    WR32(v3 + 4u, 0xE1000200u);
    v4 = RD32(0x80066E10u);
    WR32(v3, (RD32(v3) & 0xFF000000u) | (RD32(v4 + 12u) & 0x00FFFFFFu));
    res = (RD32(v4 + 12u) & 0xFF000000u) | (v3 & 0x00FFFFFFu);
    WR32(v4 + 12u, res);

    m->sp_set(m->ctx, sp + 0x28u);             /* before the return */
    return res;
}
