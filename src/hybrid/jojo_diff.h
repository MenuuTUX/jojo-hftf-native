/* Per-call differential harness (development only; never part of the final target). See ../PHASE2_DESIGN.md. */
#ifndef JOJO_DIFF_H
#define JOJO_DIFF_H
#include <stdint.h>

struct CPUState;
typedef void (*JojoGuestBody)(struct CPUState *cpu);

/* Declared outputs of a function, from its decompiled signature. Always compared: s0-s7, gp, sp, fp, ra, pc. */
#define JD_OUT_V0    1u   /* $v0 is an output */
#define JD_OUT_V1    2u   /* $v1 is an output (64-bit returns) */
#define JD_OUT_HILO  4u   /* hi/lo are outputs */

/* 1 when JOJO_DIFF names this guest address. Cheap; safe to call on every invocation. */
int  jojo_diff_armed(uint32_t addr);

/* Runs `ref` (translated reference body) and `native` from the same entry state, compares the two results and leaves the
 * native result in place. `addr` and `size` are the function's guest range (from the shard's "Size: N bytes" comment). */
void jojo_diff_call(struct CPUState *cpu, uint32_t addr, uint32_t size, uint32_t outs, JojoGuestBody ref, JojoGuestBody native);

/* Runs a generated body to return: the body, then each callee dispatched through psx_dispatch_call, then the body resumed at
 * its continuation, until pc is the entry $ra. Use it for the reference body and for any native body that wraps a reference. */
void jojo_run_to_return(struct CPUState *cpu, JojoGuestBody body, uint32_t addr, uint32_t size);

/* 1 when a wrapper is entered with pc strictly inside the function (a continuation). Counted; the caller runs the reference body. */
int  jojo_resumed(const struct CPUState *cpu, uint32_t addr, uint32_t size);


/* Synthetic fuzz (hybrid/jojo_fuzz.c). */
typedef struct { uint64_t iters, compared, mismatches, reference_only, frozen_skip, info_undeclared; uint32_t max_stack_depth; uint64_t cyc_ref, cyc_nat, mem_ops; } JojoFuzzCounts;
void jojo_diff_fuzz_call(struct CPUState *cpu, uint32_t addr, uint32_t size, uint32_t outs, JojoGuestBody ref, JojoGuestBody native,
                         JojoFuzzCounts *out);

/* For a forked, isolated compare: hang alarm in seconds, and 1 = write no RAM dumps. */
void jojo_diff_set_isolated(int alarm_s, int nodump);

#endif
