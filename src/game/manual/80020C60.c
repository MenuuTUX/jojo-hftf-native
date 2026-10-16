#include "manual.h"
#include "hex2c_rt.h"

/* 80020C60: allocate the primitive chain for the object at $a0 (object+0x28 points to its primitive list).
 * First it scans the 12-byte entries at the pointer stored at 0x80089F48 (count = halfword 0x80081BC8): an entry with
 * byte +3 nonzero, word +4 equal to object+0x28 and (byte +3 & 3) equal to (object+0x3D ^ object+0x8F) is taken: its
 * byte +2 is set to 0xFF and the entry is returned. The translation dropped this scan; without it a new chain was
 * allocated instead of the existing entry. If nothing matches: the object counter at +0x88/+0x8C, the primitive
 * count of the list at +0x28, the head node from 0x80020E7C, the chain of nodes, the link at +0x20, and the call to
 * 0x8002059C with $a0 = object. The return is the head node, or 0 on failure. $a1..$a3 of the plain calls are passed
 * as 0; 0x80020E7C reads none of them (checked: each is written before its first read). 0x8002059C reads $a0 first
 * (checked: $a1 is written before its first read; its own code never reads $a2/$a3; its handlers are not checked).
 * Note: 0x8002059C is in the blocked list (it calls 0x8002074C, which reads $t0 carried in from the caller);
 * $t0 is not modelled here. */
static uint32_t man_80020C60_body(const GuestMem *m, uint32_t s2) {
    uint32_t tbl, cnt, i, e, used, head, cur, nx, sum, p, r, n16, a0c, s1;

    /* 0x80020C6C..0x80020CE8: table scan */
    tbl = RD32(0x80089F48u);
    cnt = RD16(0x80081BC8u);
    for (i = 0u; i < cnt; i++) {
        e = tbl + i * 12u;
        used = RD8(e + 3u);
        if (used != 0u && RD32(e + 4u) == RD32(s2 + 0x28u)) {
            if ((used & 3u) == (RD8(s2 + 0x3Du) ^ RD8(s2 + 0x8Fu))) {
                WR8(e + 2u, 0xFFu);
                return e;
            }
        }
    }

    /* 0x80020CEC: object counter at +0x88 / +0x8C */
    if (RD32(s2 + 0x88u) != 0u) {
        if (RD16(s2 + 0x8Cu) != 0u) {
            return 0u;
        }
        WR16(s2 + 0x8Cu, 1u);
    }

    /* 0x80020D10..0x80020D40: sum the primitive counts of the list at +0x28 */
    p = RD32(s2 + 0x28u);
    sum = RD16(p + 8u);
    if (RD16(p + 10u) != 0u) {
        p += 10u;                   /* asm: $a0 = list+10 before the loop (the first sum read is at list+20) */
        do {
            p += 12u;
            sum += RD16(p - 2u);
        } while (RD16(p) != 0u);
    }

    /* 0x80020D44..0x80020D54: count = (sum + 7) >> 3 (the bgez fallback is kept for fidelity) */
    a0c = (sum & 0xFFFFu) + 7u;
    if ((int32_t)a0c < 0) {
        a0c = (sum & 0xFFFFu) + 14u;
    }
    s1 = (uint32_t)((int32_t)a0c >> 3);

    /* 0x80020D54: head node */
    cur = m->call4(m->ctx, 0x80020E7Cu, a0c, 0u, 0u, 0u);
    if (cur == 0u) {
        return 0u;
    }

    /* 0x80020D64..0x80020D9C: the chain is built from the head; n16 = (count - 1) as a halfword */
    n16 = (s1 - 1u) & 0xFFFFu;
    head = cur;
    WR8(head + 3u, (RD8(s2 + 0x3Du) ^ RD8(s2 + 0x8Fu)) | 0x80u);
    WR8(head + 2u, 0xFFu);
    WR32(head + 4u, RD32(s2 + 0x28u));

    /* 0x80020DA4..0x80020DD4: walk the chain, allocating missing links */
    cur = head;
    if (n16 != 0u) {
        do {
            if (RD32(cur + 8u) == 0u) {
                nx = m->call4(m->ctx, 0x80020E7Cu, 0u, 0u, 0u, 0u);
                WR32(cur + 8u, nx);
                if (nx == 0u) {
                    WR8(head + 2u, 0u);
                    WR32(head + 4u, 0u);
                    return 0u;
                }
            }
            cur = RD32(cur + 8u);
            n16 = (n16 - 1u) & 0xFFFFu;
        } while (n16 != 0u);
    }

    /* 0x80020DDC..0x80020E08: close the chain after the last node */
    nx = RD32(cur + 8u);
    if (nx != 0u) {
        WR8(nx + 3u, 0x80u);
        WR8(RD32(cur + 8u) + 2u, 0u);
        WR32(RD32(cur + 8u) + 4u, 0u);
        WR32(cur + 8u, 0u);
    }

    /* 0x80020E0C..0x80020E30: link the head at +0x20, call 0x8002059C */
    WR32(s2 + 0x20u, head);
    r = m->call4(m->ctx, 0x8002059Cu, s2, 0u, 0u, 0u);
    if (r != 0u) {
        WR8(RD32(s2 + 0x20u) + 2u, 0u);
        WR32(RD32(s2 + 0x20u) + 4u, 0u);
        WR32(s2 + 0x20u, 0u);
        return 0u;
    }
    return head;
}

uint32_t man_80020C60(const GuestMem *m, uint32_t a0) {
    uint32_t sp = m->sp_get(m->ctx) - 0x28u, r;   /* 0x28 = the original "addiu sp,sp,-40" */
    m->sp_set(m->ctx, sp);
    r = man_80020C60_body(m, a0);
    m->sp_set(m->ctx, sp + 0x28u);                /* before every return */
    return r;
}
