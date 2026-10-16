/* Scheduler 0x80029300 (172 bytes, slice step 2): harness binding and native body.
 *
 * Native body (game/scheduler.c): used when JOJO_GAME contains "sched" (unarmed), and as the native side of the harness when
 * JOJO_DIFF_CONTROL is unset or "native". Reference body: the translated function.
 * Controls (the harness proof that it can fail), each runs the reference to return (jojo_run_to_return), then changes one thing:
 *   JOJO_DIFF_CONTROL=aa            no change. Must give 0 mismatches (false-positive test).
 *   JOJO_DIFF_CONTROL=neg           flips RAM byte 0x80073B40 on every call. Must be caught on the first call (RAM path).
 *   JOJO_DIFF_CONTROL=negreg        v0 + 1. Must be caught on the first call (register path).
 * NOTE (2026-10-13): unset used to mean "aa"; it now means the real native body. Commands that need the control say "aa". */
#include "jojo_game_glue.h"
#include "scheduler.h"
#include "psx_memory.h"

#include <stdlib.h>
#include <string.h>

extern void jojo_lle_func_80029300(CPUState *cpu);
extern uint8_t *memory_get_ram_ptr(void);

#define SCHED_ADDR 0x80029300u
#define SCHED_SIZE 172u
#define CTRL_FLIP_ADDR 0x80073B40u   /* scheduler list-head table: a RAM byte this function reads, flipped on every call */

static void sched_ref(CPUState *cpu) {
    jojo_run_to_return(cpu, jojo_lle_func_80029300, SCHED_ADDR, SCHED_SIZE);
}

static void sched_real(CPUState *cpu) {
    GuestMem m = jojo_gm_for(cpu);
    uint64_t m0 = g_jojo_memops;
    /* guest cycles per call: game/cycles_extra.tsv */
    cpu->gpr[2] = sched_run_list(&m, cpu->gpr[4]);
    jojo_native_charge_own(347u, m0);
    jojo_return(cpu);
}

static void sched_native(CPUState *cpu) {
    const char *mode = getenv("JOJO_DIFF_CONTROL");
    if (!mode || !*mode || strcmp(mode, "native") == 0) { sched_real(cpu); return; }
    sched_ref(cpu);
    if (mode && strcmp(mode, "neg") == 0) {
        memory_get_ram_ptr()[CTRL_FLIP_ADDR & 0x1fffffu] ^= 1u;
    } else if (mode && strcmp(mode, "negreg") == 0) {
        cpu->gpr[2] += 1u;
    }
    jojo_return(cpu);
}

void func_80029300(CPUState *cpu) {
    if (jojo_resumed(cpu, SCHED_ADDR, SCHED_SIZE)) { jojo_lle_func_80029300(cpu); return; }
    jojo_note_call(SCHED_ADDR, cpu->gpr[31]);
    if (jojo_diff_armed(SCHED_ADDR)) {
        jojo_diff_call(cpu, SCHED_ADDR, SCHED_SIZE, JD_OUT_V0, sched_ref, sched_native);
        return;
    }
    if (!jojo_game_on("sched")) { jojo_lle_func_80029300(cpu); return; }
    sched_real(cpu);
}
