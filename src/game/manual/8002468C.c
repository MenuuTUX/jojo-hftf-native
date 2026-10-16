#include "manual.h"
#include "hex2c_rt.h"

static uint32_t action(const GuestMem *m, uint32_t actor, uint32_t a3) {
    uint32_t other = RD32(actor + 0x170u), linked = RD32(actor + 0x108u), id;
    int32_t code = RDS8(actor + 0x14Du), kind;
    if (code == -1) return 128u;
    if (code & 128) {
        kind = RDS8(linked + 0x137u);
        if (kind == 2 || kind == 12 || kind == 17) {
            if (RD8(actor + 5u) == 2u)
                return m->call4(m->ctx, 0x80024BF4u, linked, kind == 2 ? 303u : 297u, other, a3);
            return m->call4(m->ctx, 0x800241C0u, actor, RD8(actor + 0x13Au) ? 315u : 312u, other, a3);
        }
        /* 80024780 forms the table address with addiu. There is no lw
         * of 80054918, despite the decompiler's pointer-shaped expression. */
        id = RD16(0x80054918u + 2u * (RD8(actor + 0x14Du) & 127u));
        return m->call4(m->ctx, 0x8002462Cu, actor, id, other, a3);
    }
    id = RD16(0x800548E8u + 2u * (uint32_t)code);
    if ((RD8(linked + 0x13Au) || RD8(linked + 0x1Au) == 2u) && code < 3) id += 8u;
    if ((RD8(actor + 5u) == 2u || RD8(other + 5u) == 2u) && (int16_t)id < 64) {
        id = (uint16_t)(id + 64u);
        m->call4(m->ctx, 0x80024B74u, actor, id, other, a3);
    } else if (code == 7) {
        m->call4(m->ctx, 0x80024BF4u, linked, 80u, other, a3);
    } else if (RD8(actor + 0x13Au)) {
        id = (uint16_t)(id + 32u);
    }
    return m->call4(m->ctx, 0x80024104u, actor, (uint32_t)(int32_t)(int16_t)id, 0, 0);
}

uint32_t man_8002468C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx), result;
    m->sp_set(m->ctx, sp - 32u);
    result = action(m, a0, a3);
    m->sp_set(m->ctx, sp);
    return result;
}
