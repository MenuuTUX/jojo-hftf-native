/* Animation VM, native (Phase 2 slice step 1). Behavior follows the decompiled exe functions in
 * ps1-port/ida/decomp/fight/TEXT (sub_800268BC, sub_80026A04, sub_80026A60, sub_80026AFC). The handlers are guest code,
 * reached through the table at ANIM_HANDLER_TABLE with one argument (the object) in $a0. */
#include "anim_vm.h"

/* Runs the opcode loop: while the timer is zero, run the handler of the next opcode. Returns the timer value that ends the loop
 * (the original's `result`). */
static int32_t run_records(const GuestMem *m, uint32_t obj) {
    int32_t result;
    do {
        uint32_t stream = m->rd32(m->ctx, obj + ANIM_OBJ_STREAM);
        uint32_t op = m->rd8(m->ctx, stream);
        uint32_t bit6 = op & 0x40u;
        uint32_t index;
        if (op & 0x80u) { op = 0; bit6 = 0; }   /* bit 7: opcode 0 */
        index = op;
        if (bit6 != 0) index = 2;               /* bit 6 (without bit 7): opcode 2 */
        m->call1(m->ctx, m->rd32(m->ctx, ANIM_HANDLER_TABLE + 4u * index), obj);
        result = (int16_t)m->rd16(m->ctx, obj + ANIM_OBJ_TIMER);
    } while (m->rd16(m->ctx, obj + ANIM_OBJ_TIMER) == 0);
    return result;
}

int32_t anim_set(const GuestMem *m, uint32_t obj, uint32_t stream) {
    m->wr16(m->ctx, obj + ANIM_OBJ_TIMER, 0);
    m->wr8(m->ctx, obj + ANIM_OBJ_MODE, 0);
    m->wr32(m->ctx, obj + ANIM_OBJ_START, stream);
    m->wr32(m->ctx, obj + ANIM_OBJ_STREAM, stream);
    m->wr16(m->ctx, obj + ANIM_OBJ_FLAG, 0);
    return run_records(m, obj);
}

int32_t anim_advance(const GuestMem *m, uint32_t obj, uint32_t stream) {
    m->wr16(m->ctx, obj + ANIM_OBJ_TIMER, 0);
    m->wr32(m->ctx, obj + ANIM_OBJ_STREAM, stream);
    m->wr16(m->ctx, obj + ANIM_OBJ_FLAG, 0);
    return run_records(m, obj);
}

int32_t anim_tick(const GuestMem *m, uint32_t obj) {
    int32_t result = -1;
    if (m->rd16(m->ctx, obj + ANIM_OBJ_FLAG) == 0) {
        int16_t timer = (int16_t)m->rd16(m->ctx, obj + ANIM_OBJ_TIMER);
        if (timer > 0) m->wr16(m->ctx, obj + ANIM_OBJ_TIMER, (uint16_t)(timer - 1));
        result = 1;
        if (m->rd16(m->ctx, obj + ANIM_OBJ_TIMER) == 0) {
            anim_advance(m, obj, m->rd32(m->ctx, obj + ANIM_OBJ_STREAM));
            return 0;
        }
    }
    return result;
}

uint32_t anim_0x80026AFC(const GuestMem *m, uint32_t obj) {
    uint32_t flag = m->rd16(m->ctx, obj + ANIM_OBJ_FLAG);
    if (flag == 0) {
        uint32_t stream = m->rd32(m->ctx, obj + ANIM_OBJ_STREAM);
        m->wr16(m->ctx, obj + ANIM_OBJ_TIMER, 0);
        return (uint32_t)anim_advance(m, obj, stream);
    }
    return flag;
}

/* Starts stream number `index` of the object's stream list (sub_80026804). The list pointer is found two levels down from
 * obj+0x12C. Like set_anim, but it does not touch the mode byte and writes the start into obj+0x44 and the stream pointer. The
 * original reads the timer back after clearing it, so its "timer is zero" test is always true and the record loop always runs. */
uint32_t anim_idx_80026804(const GuestMem *m, uint32_t obj, uint32_t index) {
    uint32_t list = m->rd32(m->ctx, m->rd32(m->ctx, obj + 300u));
    uint32_t stream;
    m->wr16(m->ctx, obj + ANIM_OBJ_TIMER, 0);
    m->wr8(m->ctx, obj + 79u, 0);
    stream = m->rd32(m->ctx, list + 4u * index);
    m->wr16(m->ctx, obj + ANIM_OBJ_FLAG, 0);
    m->wr32(m->ctx, obj + ANIM_OBJ_START, stream);
    m->wr32(m->ctx, obj + ANIM_OBJ_STREAM, stream);
    return (uint32_t)run_records(m, obj);
}
