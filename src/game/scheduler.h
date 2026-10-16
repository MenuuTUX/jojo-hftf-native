/* Object scheduler (Phase 2 slice step 2). Native rewrite of sub_80029300 (ps1-port/ida/decomp/fight/TEXT/80029300.c): walks the
 * linked list of game objects of one kind and runs each object's behavior handler. The handlers stay translated and are reached
 * through GuestMem.call1. */
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "guest_mem.h"

#define SCHED_LIST_HEADS 0x80073B40u    /* u32 per kind: first object of that kind's list, 0 when empty */
#define SCHED_KIND_BASE  0x8005AA4Cu    /* u16 per kind: first index of that kind's handlers in the handler table */
#define SCHED_HANDLERS   0x8005AA8Cu    /* u32 per index: handler address, called with the object address */
#define SCHED_OBJ_ACTIVE 0u             /* u8: non-zero means the object runs this frame */
#define SCHED_OBJ_CLASS  2u             /* u8: added to the kind's base to pick the handler */
#define SCHED_OBJ_NEXT   92u            /* u32: next object of the list */

/* Runs one list. Returns what the original leaves in $v0: 0x80060000 for an empty list, else the last handler result, or
 * 0 when the last object was not active. */
uint32_t sched_run_list(const GuestMem *m, uint32_t kind);

#endif
