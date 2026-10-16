#include "manual.h"
#include "hex2c_rt.h"

/* 80020F20: $a0 = dest (object-side block), $a2 = source object. Stores the source flags at dest+0x40/+0x41 and the
 * type byte at dest+0x3C, then calls the table function at 0x8005A76C[type & 0xF] with $a0..$a3 passed on unchanged
 * (the original does not write any argument register before that jalr). If it returns nonzero and (type & 7) < 3,
 * and dest+0x40 is set, 0x80023540 is called with $a0 = the return value, $a1 = the source object, $a2 = dest+0x41.
 * The return is the table result, or the 0x80023540 result when that is nonzero, else 0.
 * $a3 is not passed to 0x80023540: that callee writes $a3 before any read (checked), so the value is not used.
 * Bytes at +0xB0/+0xB8/+0x40/+0x41 are re-read after the table call, as the original does. */
static uint32_t man_80020F20_body(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t r, v7, typ;

    WR8(a0 + 64u, RD8(a2 + 184u) >> 7);
    WR8(a0 + 65u, RD8(a2 + 184u) & 0x60u);
    typ = RD8(a2 + 176u);
    WR8(a0 + 60u, typ);

    r = m->calli(m->ctx, RD32(0x8005A76Cu + ((typ & 0xFu) << 2)), a0, a1, a2, a3);
    v7 = r;

    if (r != 0u && (RD8(a2 + 176u) & 7u) < 3u) {
        if (RD8(a0 + 64u) == 0u) {
            return v7;
        }
        v7 = m->call4(m->ctx, 0x80023540u, r, a2, RD8(a0 + 65u), 0u);
        if (v7 != 0u) {
            return v7;
        }
        return 0u;
    }
    return r;
}

uint32_t man_80020F20(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx) - 0x20u, r;   /* 0x20 = the original "addiu sp,sp,-32" */
    m->sp_set(m->ctx, sp);
    r = man_80020F20_body(m, a0, a1, a2, a3);
    m->sp_set(m->ctx, sp + 0x20u);                /* before every return */
    return r;
}
