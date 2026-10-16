/* Hit test, native (Phase 2 slice step 3). See hit.h. Behavior follows the Hex-Rays pseudocode of the three functions. All 16-bit
 * sums are reduced modulo 2^16 before they are compared, as in the original. */
#include "hit.h"

#define RD8(a)  m->rd8(m->ctx, (a))
#define RD16(a) m->rd16(m->ctx, (a))
#define RD32(a) m->rd32(m->ctx, (a))

uint32_t hit_can_be_hit(const GuestMem *m, uint32_t obj, uint32_t which) {
    uint32_t mode = RD8(HIT_MODE_BYTE);
    if (mode != 0 && mode != 3) return 1;
    if ((which & 0xFFu) != 0) return (RD8(obj + 323) & 2u) == 0;
    return (RD8(obj + 323) & 1u) == 0;
}

uint32_t hit_box_overlap(const GuestMem *m, uint32_t attacker, uint32_t defender, uint32_t attacker_box, uint32_t defender_box) {
    int32_t ia = (int16_t)attacker_box, id = (int16_t)defender_box;
    uint32_t a, d, ax, aw, ay, ah, dx, dw, dy, dh, a_left, d_left;
    if (ia == 0) return 0;
    if (id == 0 || hit_can_be_hit(m, defender, 0) == 0) return 0;
    a = RD32(HIT_BOX_TABLES + 4u * (uint32_t)(int32_t)(int8_t)RD8(attacker + 310)) + 8u * (uint32_t)ia;
    d = RD32(HIT_BOX_TABLES + 4u * (uint32_t)(int32_t)(int8_t)RD8(defender + 310)) + 8u * (uint32_t)id;
    ax = RD16(a); aw = RD16(a + 2); ay = RD16(a + 4); ah = RD16(a + 6);
    dx = RD16(d); dw = RD16(d + 2); dy = RD16(d + 4); dh = RD16(d + 6);
    a_left = RD8(attacker + 11) != 0 ? RD16(attacker + 106) - (aw + ax) : RD16(attacker + 106) + ax;
    d_left = RD8(defender + 11) != 0 ? RD16(defender + 106) - (dw + dx) : RD16(defender + 106) + dx;
    return ((aw + dw) & 0xFFFFu) >= ((d_left - a_left + dw) & 0xFFFFu) &&
           ((ah + dh) & 0xFFFFu) >= ((RD16(defender + 110) + dy - (RD16(attacker + 110) + ay) + dh) & 0xFFFFu);
}

uint32_t hit_test(const GuestMem *m, uint32_t attacker, uint32_t defender, uint32_t out) {
    uint32_t row_a, row_d, ch;
    int enter;
    ch = (uint32_t)(int32_t)(int8_t)RD8(attacker + 310);
    enter = RD8(RD32(RD32(attacker + 300) + 24) + 48u * RD8(attacker + 312) + 5u) != 30 ||
            RD8(HIT_PAIR_TABLE + 4u * (228u * ch + 1u) + 1u) == 1;
    if (!enter) return 0;
    if (RD8(attacker + 313) != 0) return 0;
    if (RD8(attacker + 361) != 0) return 0;
    if (RD8(HIT_MODE_BYTE) != 0 && (RD8(HIT_MODE_BYTE) & (ch + 1u)) == 0) return 0;
    row_a = RD32(RD32(attacker + 300) + 20) + 12u * RD16(attacker + 256);
    row_d = RD32(RD32(defender + 300) + 20) + 12u * RD16(defender + 256);
    m->wr8(m->ctx, out, 0);
    if (hit_box_overlap(m, attacker, defender, RD16(row_a), RD16(row_d + 4))) m->wr8(m->ctx, out, RD8(out) | 1u);
    if (hit_box_overlap(m, attacker, defender, RD16(row_a), RD16(row_d + 6))) m->wr8(m->ctx, out, RD8(out) | 2u);
    if (hit_box_overlap(m, attacker, defender, RD16(row_a), RD16(row_d + 8))) m->wr8(m->ctx, out, RD8(out) | 4u);
    return RD8(out) != 0;
}
