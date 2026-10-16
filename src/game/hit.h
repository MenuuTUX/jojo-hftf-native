/* Hit test (Phase 2 slice step 3). Native rewrite of the AC_MAIN functions sub_800AA8E8 (test one attacker against one defender),
 * sub_800AAA9C (one box overlap) and sub_800ABB0C (may this object be hit). ps1-port/ida/decomp/fight/AC_MAIN. Objects are
 * fighter objects (offsets below, from the decompiled code; meanings are placeholders until FIELDS.md proves them). */
#ifndef HIT_H
#define HIT_H

#include "guest_mem.h"

#define HIT_BOX_TABLES   0x800CAFB0u  /* u32 per character index: table of 8-byte boxes {s16 x, w, y, h} */
#define HIT_MODE_BYTE    0x80066CA2u  /* global mode byte (0 normal) */
#define HIT_PAIR_TABLE   0x800CDD50u  /* u32 words, 228 per character index */

/* sub_800ABB0C(obj, which) */
uint32_t hit_can_be_hit(const GuestMem *m, uint32_t obj, uint32_t which);
/* sub_800AAA9C(attacker, defender, attacker_box, defender_box) */
uint32_t hit_box_overlap(const GuestMem *m, uint32_t attacker, uint32_t defender, uint32_t attacker_box, uint32_t defender_box);
/* sub_800AA8E8(attacker, defender, out): out byte gets one bit per defender box that overlaps; returns non-zero if any */
uint32_t hit_test(const GuestMem *m, uint32_t attacker, uint32_t defender, uint32_t out);

#endif
