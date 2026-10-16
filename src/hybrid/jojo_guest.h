/* Hybrid glue helpers for guest-function wrappers (development only). */
#ifndef JOJO_GUEST_H
#define JOJO_GUEST_H
#include "cpu_state.h"
#include "interrupts.h"
#include "psx_cyc.h"

/* Return to the caller the way generated code does at `jr ra`: flush batched cycles, let a pending interrupt be taken, then continue at ra. */
static inline void jojo_return(CPUState *cpu) {
    uint32_t ra = cpu->gpr[31];
    psx_cyc_bb_defer_flush();
    psx_check_interrupts_at(cpu, ra);
    cpu->pc = ra;
}
#endif
