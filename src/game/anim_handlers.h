/* Animation opcode handlers (Phase 2 slice step 1). Native rewrite of the 21 exe functions the VM calls through the table at
 * ANIM_HANDLER_TABLE, 0x80026BA0 to 0x80027388 (ps1-port/ida/decomp/fight/TEXT, ANIMATION_FORMAT.md). Each takes the animation
 * object and returns the value the original leaves in $v0 (no caller reads it). Names are placeholders: anim_h_<address>. */
#ifndef ANIM_HANDLERS_H
#define ANIM_HANDLERS_H

#include "anim_vm.h"

#define ANIM_RNG_FN 0x80025BF8u    /* guest function: advances the random state word at 0x80066C62, returns its low byte */
#define ANIM_SETUP_FN 0x80024328u  /* guest function (obj, row byte 9): applies a row action, still translated */

uint32_t anim_h_80026BA0(const GuestMem *m, uint32_t obj);  /* full record: timer, cell, attrs, row copy, move */
uint32_t anim_h_80026DB8(const GuestMem *m, uint32_t obj);  /* jump to the 32-bit stream address in the record */
uint32_t anim_h_80026DDC(const GuestMem *m, uint32_t obj);  /* short record: timer, cell, attrs */
uint32_t anim_h_80026E50(const GuestMem *m, uint32_t obj);
uint32_t anim_h_80026EC0(const GuestMem *m, uint32_t obj);
uint32_t anim_h_80026F30(const GuestMem *m, uint32_t obj);
uint32_t anim_h_80026F50(const GuestMem *m, uint32_t obj);
uint32_t anim_h_80026F80(const GuestMem *m, uint32_t obj);
uint32_t anim_h_80026FB8(const GuestMem *m, uint32_t obj);
uint32_t anim_h_80027008(const GuestMem *m, uint32_t obj);
uint32_t anim_h_80027040(const GuestMem *m, uint32_t obj);
uint32_t anim_h_80027084(const GuestMem *m, uint32_t obj);
uint32_t anim_h_800270B4(const GuestMem *m, uint32_t obj);
uint32_t anim_h_800270C8(const GuestMem *m, uint32_t obj);
uint32_t anim_h_80027138(const GuestMem *m, uint32_t obj);
uint32_t anim_h_80027184(const GuestMem *m, uint32_t obj);
uint32_t anim_h_800271EC(const GuestMem *m, uint32_t obj);
uint32_t anim_h_8002721C(const GuestMem *m, uint32_t obj);
uint32_t anim_h_8002723C(const GuestMem *m, uint32_t obj);
uint32_t anim_h_8002726C(const GuestMem *m, uint32_t obj);
uint32_t anim_h_80027368(const GuestMem *m, uint32_t obj);

#endif
