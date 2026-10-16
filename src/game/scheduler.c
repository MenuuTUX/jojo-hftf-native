/* Object scheduler, native (Phase 2 slice step 2). See scheduler.h. */
#include "scheduler.h"

uint32_t sched_run_list(const GuestMem *m, uint32_t kind) {
    uint32_t obj, result = 0x80060000u;
    kind &= 0xFFu;
    obj = m->rd32(m->ctx, SCHED_LIST_HEADS + 4u * kind);
    while (obj != 0) {
        result = m->rd8(m->ctx, obj + SCHED_OBJ_ACTIVE);
        if (result != 0) {
            uint32_t index = m->rd16(m->ctx, SCHED_KIND_BASE + 2u * kind) + m->rd8(m->ctx, obj + SCHED_OBJ_CLASS);
            result = m->call1(m->ctx, m->rd32(m->ctx, SCHED_HANDLERS + 4u * index), obj);
        }
        obj = m->rd32(m->ctx, obj + SCHED_OBJ_NEXT);
    }
    return result;
}
