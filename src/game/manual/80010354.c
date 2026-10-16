#include "manual.h"
#include "hex2c_rt.h"

/* 80010354: reset sequence before the boot thread. ResetCallback(a0..a3 on entry, passed on unchanged), StopCallback,
 * then 80036004(0), 80039C08, 80035D20(0), 8003CFF0(0), 80012CF0, 80010F60, 800170A4, 80023860, 80023924, and finally
 * 8003C530(0x80010BD4), whose $v0 is the return value. Every call after the first gets zero arguments (the callee
 * clobbers $a0..$a3). */
uint32_t man_80010354(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    m->call4(m->ctx, 0x8003C4A0u, a0, a1, a2, a3);
    m->call4(m->ctx, 0x8003C594u, 0, 0, 0, 0);
    m->call4(m->ctx, 0x80036004u, 0, 0, 0, 0);
    m->call4(m->ctx, 0x80039C08u, 0, 0, 0, 0);
    m->call4(m->ctx, 0x80035D20u, 0, 0, 0, 0);
    m->call4(m->ctx, 0x8003CFF0u, 0, 0, 0, 0);
    m->call4(m->ctx, 0x80012CF0u, 0, 0, 0, 0);
    m->call4(m->ctx, 0x80010F60u, 0, 0, 0, 0);
    m->call4(m->ctx, 0x800170A4u, 0, 0, 0, 0);
    m->call4(m->ctx, 0x80023860u, 0, 0, 0, 0);
    m->call4(m->ctx, 0x80023924u, 0, 0, 0, 0);
    return m->call4(m->ctx, 0x8003C530u, 0x80010BD4u, 0, 0, 0);
}
