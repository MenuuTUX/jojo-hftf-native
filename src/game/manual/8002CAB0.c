#include "manual.h"
#include "hex2c_rt.h"

/* 8002CAB0 (TEXT). No inputs. Returns 0 (in $v0 at jr ra).
 * Section 1: 2 outer x 4 inner over pointers 0x800660E8 + 12 (+64 per outer), step 16:
 *   calls 80039940(p) then 80039890(p, 0).
 * Section 2: 2 outer x 4 inner over pointers 0x800660E8 + 140 (+144 per outer), step 36:
 *   calls 800398E0(p) then 80039890(p, 0).
 * The callees 80039940, 800398E0 read only $a0 and 80039890 reads only $a0 and $a1, so the other
 * argument registers are passed as 0. */
uint32_t man_8002CAB0(const GuestMem *m) {
    uint32_t base = 0x800660E8u;
    uint32_t off;
    uint32_t p;
    uint32_t outer;
    uint32_t inner;

    off = 12u;
    for (outer = 0; outer < 2u; outer++) {
        p = base + off;
        for (inner = 0; inner < 4u; inner++) {
            m->call4(m->ctx, 0x80039940u, p, 0u, 0u, 0u);
            m->call4(m->ctx, 0x80039890u, p, 0u, 0u, 0u);
            p += 16u;
        }
        off += 64u;
    }

    off = 140u;
    for (outer = 0; outer < 2u; outer++) {
        p = base + off;
        for (inner = 0; inner < 4u; inner++) {
            m->call4(m->ctx, 0x800398E0u, p, 0u, 0u, 0u);
            m->call4(m->ctx, 0x80039890u, p, 0u, 0u, 0u);
            p += 36u;
        }
        off += 144u;
    }

    return 0u;
}
