/* Animation opcode handlers, native (Phase 2 slice step 1). Behavior follows the Hex-Rays pseudocode in
 * ps1-port/ida/decomp/fight/TEXT/<address>.c, including the order of reads and writes. Object offsets are the decimal offsets
 * used there; the stream pointer is the u32 at +60 (0x3C). */
#include "anim_handlers.h"

#define RD8(a)   m->rd8(m->ctx, (a))
#define RD16(a)  m->rd16(m->ctx, (a))
#define RD32(a)  m->rd32(m->ctx, (a))
#define WR8(a, v)  m->wr8(m->ctx, (a), (uint8_t)(v))
#define WR16(a, v) m->wr16(m->ctx, (a), (uint16_t)(v))
#define WR32(a, v) m->wr32(m->ctx, (a), (uint32_t)(v))

#define PC(o)      ((o) + 60u)    /* stream pointer */
#define PREV(o)    ((o) + 64u)    /* start of the record just run */
#define RET(o)     ((o) + 52u)    /* return address of the call opcode */
#define TIMER(o)   ((o) + 40u)
#define COUNT(o)   ((o) + 79u)    /* records run since the last jump */
#define VAR(o, i)  ((o) + (i) + 42u)  /* byte variables, index 0 is at +42 */

uint32_t anim_h_80026BA0(const GuestMem *m, uint32_t obj) {
    uint32_t stream = RD32(PC(obj));
    uint32_t b0 = RD8(stream), type, row, adv, b15, b5, cur, t;
    int32_t dx;
    uint16_t x;
    WR16(TIMER(obj), RD8(stream + 1));
    WR16(obj + 16, RD16(RD32(PC(obj)) + 2));
    WR8(obj + 77, RD8(RD32(PC(obj)) + 4));
    WR8(obj + 78, RD8(RD32(PC(obj)) + 5));
    type = RD8(obj + 26);
    adv = b0 & 0x2Fu;
    if (type < 3 || type == 7 || type == 6) {
        uint32_t s = RD32(PC(obj));
        row = RD32(RD32(obj + 300) + 4) + 16u * (uint32_t)(int32_t)(int16_t)RD16(s + 8);
        WR16(obj + 256, RD16(s + 6));
        WR8(obj + 315, RD8(row));
        WR8(obj + 318, RD8(row + 3));
        WR8(obj + 319, RD8(row + 4));
        WR8(obj + 316, RD8(row + 1));
        WR8(obj + 317, RD8(row + 2));
        {
            uint32_t b11 = RD8(row + 11);
            WR8(obj + 322, RD8(obj + 321));
            WR8(obj + 323, b11);
        }
        WR8(obj + 321, RD8(row + 8));
        WR8(obj + 187, RD8(row + 12));
        WR8(obj + 320, RD8(row + 10));
        if (RD8(row + 9) != 0) m->call2(m->ctx, ANIM_SETUP_FN, obj, RD8(row + 9));
    } else {
        row = RD32(obj + 72) + 16u * (uint32_t)(int32_t)(int16_t)RD16(RD32(PC(obj)) + 8);
    }
    b15 = RD8(row + 15);
    WR8(obj + 18, b15);
    b5 = RD8(row + 5);
    if (b5 != 0) {
        WR8(obj + 248, RD8(obj + 11) == (b15 & 1u) ? b5 : (uint32_t)(0u - b5));
    }
    dx = (int8_t)RD8(row + 13);
    x = RD16(obj + 106);
    x = (uint16_t)(RD8(obj + 11) != 0 ? x - dx : x + dx);
    WR16(obj + 106, x);
    t = RD16(TIMER(obj));
    WR16(obj + 110, RD16(obj + 110) + (int32_t)(int8_t)RD8(row + 14));
    WR16(obj + 34, (t >> 7) & 1u);
    WR16(TIMER(obj), RD16(TIMER(obj)) & 0x7Fu);
    cur = RD32(PC(obj));
    WR32(PREV(obj), cur);
    WR32(PC(obj), cur + adv);
    WR8(COUNT(obj), RD8(COUNT(obj)) + 1);
    return cur + adv;
}

uint32_t anim_h_80026DB8(const GuestMem *m, uint32_t obj) {
    uint32_t s = RD32(PC(obj)), r;
    WR16(TIMER(obj), 0);
    WR8(COUNT(obj), 0);
    r = (RD16(s + 4) << 16) + RD16(s + 6);
    WR32(PC(obj), r);
    return r;
}

uint32_t anim_h_80026DDC(const GuestMem *m, uint32_t obj) {
    uint32_t s = RD32(PC(obj)), b0 = RD8(s), s2, b5, r;
    WR16(TIMER(obj), RD8(s + 1));
    s2 = RD32(PC(obj));
    WR16(obj + 16, RD16(s + 2));
    WR8(obj + 77, RD8(s2 + 4));
    b5 = RD8(s2 + 5);
    WR16(obj + 34, (RD16(TIMER(obj)) & 0x80u) != 0);
    {
        uint32_t t = RD16(TIMER(obj));
        WR32(PREV(obj), s2);
        WR8(obj + 78, b5);
        WR16(TIMER(obj), t & 0x7Fu);
    }
    r = s2 + (b0 & 0x2Fu);
    WR32(PC(obj), r);
    WR8(COUNT(obj), RD8(COUNT(obj)) + 1);
    return r;
}

/* Skips the opcode when variable 0 is non-zero; otherwise takes the jump (word at +4 of the 4-aligned record). */
static uint32_t skip_or_jump(const GuestMem *m, uint32_t obj, int jump_when_set) {
    uint32_t r;
    if ((RD8(obj + 42) != 0) == (jump_when_set != 0)) {
        uint32_t s = RD32(PC(obj));
        WR16(TIMER(obj), 0);
        WR8(COUNT(obj), 0);
        if ((s & 3u) != 0) WR32(PC(obj), s + 2);
        r = RD32(RD32(PC(obj)) + 4);
        WR32(PC(obj), r);
    } else {
        uint32_t s = RD32(PC(obj));
        r = s + RD8(s + 1);
        if ((s & 3u) != 0) r += 2;
        WR32(PC(obj), r);
    }
    return r;
}
uint32_t anim_h_80026E50(const GuestMem *m, uint32_t obj) { return skip_or_jump(m, obj, 1); }
uint32_t anim_h_80026EC0(const GuestMem *m, uint32_t obj) { return skip_or_jump(m, obj, 0); }

uint32_t anim_h_80026F30(const GuestMem *m, uint32_t obj) {
    uint32_t s = RD32(PC(obj)), r = s + (RD16(s) >> 8);
    WR8(obj + 42, 0);
    WR32(PC(obj), r);
    return r;
}

uint32_t anim_h_80026F50(const GuestMem *m, uint32_t obj) {
    uint32_t s = RD32(PC(obj)), len = RD16(s) >> 8, r;
    WR8(VAR(obj, RD8(s + 2)), RD8(s + 3));
    r = RD32(PC(obj)) + len;
    WR32(PC(obj), r);
    return r;
}

uint32_t anim_h_80026F80(const GuestMem *m, uint32_t obj) {
    uint32_t s = RD32(PC(obj)), add = RD8(s + 3), at = VAR(obj, RD8(s + 2)), len = RD16(s) >> 8, r;
    WR8(at, RD8(at) + add);
    r = RD32(PC(obj)) + len;
    WR32(PC(obj), r);
    return r;
}

uint32_t anim_h_80026FB8(const GuestMem *m, uint32_t obj) {
    uint32_t s = RD32(PC(obj)), r;
    int32_t v = (int8_t)RD8(VAR(obj, RD8(s + 2)));
    uint32_t len = RD8(s + 1);
    WR8(obj + 42, v == (int8_t)RD8(s + 3) ? 0xFFu : 0u);
    r = RD32(PC(obj)) + len;
    WR32(PC(obj), r);
    return r;
}

uint32_t anim_h_80027008(const GuestMem *m, uint32_t obj) {
    uint32_t s = RD32(PC(obj)), mask = RD8(s + 3), at = VAR(obj, RD8(s + 2)), len = RD16(s) >> 8, r;
    WR8(at, RD8(at) & mask);
    r = RD32(PC(obj)) + len;
    WR32(PC(obj), r);
    return r;
}

uint32_t anim_h_80027040(const GuestMem *m, uint32_t obj) {
    uint32_t s = RD32(PC(obj)), len = RD8(s + 1), val = RD8(s + 3), at = VAR(obj, RD8(s + 2)), r;
    if (val == 0) WR8(at, RD8(obj + 77));
    r = RD32(PC(obj)) + len;
    WR32(PC(obj), r);
    return r;
}

uint32_t anim_h_80027084(const GuestMem *m, uint32_t obj) {
    uint32_t s = RD32(PC(obj)), r;
    WR16(TIMER(obj), 0);
    WR8(COUNT(obj), 0);
    WR32(RET(obj), s + 8);
    r = (RD16(s + 4) << 16) + RD16(s + 6);
    WR32(PC(obj), r);
    return r;
}

uint32_t anim_h_800270B4(const GuestMem *m, uint32_t obj) {
    uint32_t r = RD32(RET(obj));
    WR16(TIMER(obj), 0);
    WR8(COUNT(obj), 0);
    WR32(PC(obj), r);
    return r;
}

uint32_t anim_h_800270C8(const GuestMem *m, uint32_t obj) {
    uint32_t s = RD32(PC(obj)), idx = RD8(s + 2), mask = RD8(s + 3), len = RD8(s + 1), rnd, r;
    rnd = m->call1(m->ctx, ANIM_RNG_FN, obj);
    WR8(VAR(obj, idx), mask & rnd);
    r = RD32(PC(obj)) + len;
    WR32(PC(obj), r);
    return r;
}

/* Computed jump: table of u32 targets after the (4-aligned) record, indexed by variable [halfword at +2]. */
uint32_t anim_h_80027138(const GuestMem *m, uint32_t obj) {
    uint32_t s = RD32(PC(obj)), len = RD16(s) >> 8, var = RD16(s + 2), tbl, r;
    WR16(TIMER(obj), 0);
    tbl = s + len;
    WR8(COUNT(obj), 0);
    if ((s & 3u) != 0) tbl += 2;
    r = RD32(tbl + 4u * RD8(VAR(obj, var)));
    WR32(PC(obj), r);
    return r;
}

uint32_t anim_h_80027184(const GuestMem *m, uint32_t obj) {
    uint32_t s = RD32(PC(obj)), len = RD16(s) >> 8, s2, b5, r;
    WR16(TIMER(obj), RD16(s + 2));
    s2 = RD32(PC(obj));
    WR8(obj + 77, RD8(s + 4));
    b5 = RD8(s2 + 5);
    WR16(obj + 34, (RD16(TIMER(obj)) & 0x8000u) != 0);
    {
        uint32_t t = RD16(TIMER(obj));
        WR32(PREV(obj), s2);
        WR8(obj + 78, b5);
        WR16(TIMER(obj), t & 0x7FFFu);
    }
    r = s2 + len;
    WR32(PC(obj), r);
    WR8(COUNT(obj), RD8(COUNT(obj)) + 1);
    return r;
}

uint32_t anim_h_800271EC(const GuestMem *m, uint32_t obj) {
    uint32_t s = RD32(PC(obj)), len = RD16(s) >> 8, r;
    WR8(VAR(obj, RD16(s + 2)), RD8(obj + 539));
    r = RD32(PC(obj)) + len;
    WR32(PC(obj), r);
    return r;
}

uint32_t anim_h_8002721C(const GuestMem *m, uint32_t obj) {
    uint32_t s = RD32(PC(obj)), len = RD8(s + 1);
    WR32(PC(obj), s + len);
    return len;
}

uint32_t anim_h_8002723C(const GuestMem *m, uint32_t obj) {
    uint32_t s = RD32(PC(obj)), len = RD16(s) >> 8, r;
    WR8(VAR(obj, RD16(s + 2)), RD8(obj + 507));
    r = RD32(PC(obj)) + len;
    WR32(PC(obj), r);
    return r;
}

uint32_t anim_h_8002726C(const GuestMem *m, uint32_t obj) {
    uint32_t s = RD32(PC(obj)), len = RD16(s) >> 8, s2, tbl, p, r;
    WR16(TIMER(obj), RD16(s + 2));
    s2 = RD32(PC(obj));
    WR16(obj + 16, RD16(s + 4));
    WR8(obj + 77, RD8(s2 + 6));
    WR8(obj + 78, RD8(s2 + 7));
    WR8(obj + 264, RD8(s2 + 8));
    WR8(obj + 265, RD8(s2 + 9));
    tbl = RD32(obj + 300);
    p = tbl + 8u * (uint32_t)(int32_t)(int8_t)RD8(s2 + 8);
    {
        uint32_t hi = RD32(p + 4);
        WR32(obj + 284, RD32(p));
        WR32(obj + 288, hi);
    }
    p = RD32(obj + 300) + 8u * (uint32_t)(int32_t)(int8_t)RD8(RD32(PC(obj)) + 9);
    {
        uint32_t hi = RD32(p + 4);
        WR32(obj + 292, RD32(p));
        WR32(obj + 296, hi);
    }
    WR16(obj + 34, (RD16(TIMER(obj)) & 0x80u) != 0);
    s2 = RD32(PC(obj));
    WR16(TIMER(obj), RD16(TIMER(obj)) & 0x7Fu);
    WR32(PREV(obj), s2);
    r = s2 + len;
    WR32(PC(obj), r);
    WR8(COUNT(obj), RD8(COUNT(obj)) + 1);
    return r;
}

uint32_t anim_h_80027368(const GuestMem *m, uint32_t obj) {
    uint32_t s = RD32(PC(obj)), r;
    WR16(TIMER(obj), 0);
    WR8(COUNT(obj), 0);
    r = (uint32_t)((int32_t)(int16_t)RD16(s + 6) + 4);
    WR32(PC(obj), s + r);
    return r;
}
