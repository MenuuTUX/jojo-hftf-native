/* Synthetic differential fuzz for native game functions (development only). See ../PHASE2_DESIGN.md section 9. */
#ifndef JOJO_FUZZ_H
#define JOJO_FUZZ_H
#include "jojo_diff.h"

/* One fuzzable function: the harness triple of the glue file plus the kind of entry state to make up. */
typedef struct {
    uint32_t addr, size, outs;
    JojoGuestBody ref, native;
    int kind;               /* JOJO_FUZZ_* below */
    uint32_t ptrmask;       /* JOJO_FUZZ_OBJS: bit i set = argument i is used as a base address (an object) */
    uint32_t nargs;
    int (*matches)(void);   /* overlay identity and function bytes in the restored RAM; NULL for resident code */
} JojoFuzzFn;

#define JOJO_FUZZ_ANIM_OBJ 1   /* $a0 = animation object whose stream pointer, row/table pointers and flags are random but valid */

#define JOJO_FUZZ_OBJS 2       /* arguments used as base addresses are the live fighter objects (alternating), the others small integers; state is the real state at the fuzz frame */

void jojo_fuzz_register(const char *group, const JojoFuzzFn *fns, uint32_t n);
/* Called by every generated wrapper. Runs the fuzz once, at the first call at or after frame JOJO_FUZZ_FRAME, when JOJO_FUZZ names
 * one or more registered groups (comma list). JOJO_FUZZ_ITERS = iterations per function (default 2000), JOJO_FUZZ_SEED. */
void jojo_fuzz_maybe(struct CPUState *cpu);
/* 1 when JOJO_FUZZ names the group (the glue then registers its hooks). */
int jojo_fuzz_wants(const char *group);
/* The registered entry matching the guest address and current RAM, or NULL. Restore capture RAM before lookup. */
const JojoFuzzFn *jojo_fuzz_find(uint32_t addr);
const char *jojo_fuzz_group(const JojoFuzzFn *fn);
#endif
