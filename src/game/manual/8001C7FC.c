#include "manual.h"
#include "hex2c_rt.h"

/* 8001C7FC (TEXT). Reads $a0 (index) and $a1 (selector). $a1 is passed on unchanged to 8003F940 and 80023A1C. */
uint32_t man_8001C7FC(const GuestMem *m, uint32_t a0, uint32_t a1) {
    uint32_t s1 = a0;
    uint32_t s2 = a1;
    int v4;

    m->call4(m->ctx, 0x8003F940u, 0u, a1, 0u, 0u);
    m->call4(m->ctx, 0x80023A1Cu, 1u, a1, 0u, 0u);
    v4 = (int32_t)s1 < 32;
    if (s2 != 0xFFFFFFFFu) {
        m->call4(m->ctx, 0x80023A1Cu, 2u, a1, 0u, 0u);
        v4 = (int32_t)s1 < 32;
    }
    if (v4) {
        if (s2 == 0xFFFFFFFFu)
            return m->call4(m->ctx, 0x800E0B84u, RD16(0x80059FE8u + (s1 << 1)), 0u, 0u, 0u);
        m->call4(m->ctx, 0x8001EDDCu, RD16(0x80059FE8u + (s1 << 1)), 0u, 0u, 0u);
        return m->call4(m->ctx, 0x8001EDDCu, RD16(0x80059FE8u + (s2 << 1)), 1u, 0u, 0u);
    }
    return m->call4(m->ctx, 0x8001EDDCu, RD16(0x8005A028u + ((s1 - 33u) << 1)), 0u, 0u, 0u);
}
