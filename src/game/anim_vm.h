/* Animation VM (Phase 2 slice step 1). Native rewrite of the exe functions in ps1-port/ida/decomp/fight/TEXT:
 *   set_anim  0x800268BC   (sub_800268BC)
 *   tick      0x80026A04   (sub_80026A04)
 *   advance   0x80026A60   (sub_80026A60)
 *   0x80026AFC             (sub_80026AFC: calls advance when the flag at obj+0x22 is zero)
 * Handlers stay translated: each one is called through the guest table at ANIM_HANDLER_TABLE. */
#ifndef ANIM_VM_H
#define ANIM_VM_H

#include "guest_mem.h"

#define ANIM_HANDLER_TABLE 0x8005A8D8u   /* 4-byte guest pointers, indexed by opcode 0..63 (READ from sub_800268BC) */

/* Object fields used by the VM (byte offsets from the object base). */
#define ANIM_OBJ_FLAG    0x22u   /* u16: non-zero stops the tick */
#define ANIM_OBJ_TIMER   0x28u   /* u16: frames left on the current record */
#define ANIM_OBJ_STREAM  0x3Cu   /* u32: current bytecode pointer */
#define ANIM_OBJ_START   0x44u   /* u32: stream start, written by set_anim */
#define ANIM_OBJ_MODE    0x4Fu   /* u8: cleared by set_anim */

int32_t anim_set(const GuestMem *m, uint32_t obj, uint32_t stream);
int32_t anim_advance(const GuestMem *m, uint32_t obj, uint32_t stream);
int32_t anim_tick(const GuestMem *m, uint32_t obj);
uint32_t anim_0x80026AFC(const GuestMem *m, uint32_t obj);
uint32_t anim_idx_80026804(const GuestMem *m, uint32_t obj, uint32_t index);   /* sub_80026804: start stream `index` */

#endif
