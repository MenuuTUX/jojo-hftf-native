/* Capture and replay of real entry states (development only). See ../PHASE2_DESIGN.md section 11.
 *
 * Capture (JOJO_CAPTURE=<file>): every hooked overlay function whose code guard passes is sampled at entry (CPU registers, RAM,
 * scratchpad, INTC state) and the original code then runs as if nothing were hooked, so the game keeps the original timeline. One
 * capture run per route replaces one route run per batch.
 *
 * Replay (JOJO_REPLAY=<file>): at the first hooked call at or after frame JOJO_REPLAY_FRAME (default 0), every captured entry state
 * is loaded in turn and the reference and native bodies are compared from it (jojo_diff_fuzz_call: no clock charge). Then the
 * process exits. Evidence class REPLAY: real game states, compared outside the live timeline. */
#ifndef JOJO_REPLAY_H
#define JOJO_REPLAY_H
#include <stdint.h>

struct CPUState;

int  jojo_capture_on(void);
int  jojo_capture_live(void);   /* capture native entry states, then keep the configured native bindings enabled */
int  jojo_replay_on(void);
/* Capture mode: called by jojo_ovl_enter after the code guard passed; samples this entry state when the sampling rule says so. */
void jojo_capture_entry(struct CPUState *cpu, uint32_t addr);
/* Replay mode: called first by jojo_fuzz_maybe (every hooked call); runs the whole replay once and exits the process. */
void jojo_replay_maybe(struct CPUState *cpu);

#endif
