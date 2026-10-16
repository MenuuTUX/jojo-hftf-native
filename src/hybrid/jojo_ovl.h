/* Overlay function hooks for Phase 2 (development only). Overlay code (AC_MAIN, PL00, PL02X) has no generated body, so a native
 * replacement is attached through the runtime's game function-entry filter (mod_runtime.cpp, psx_game_register_netplay_function_filter,
 * run offline with PSX_GAME_FILTER_OFFLINE_PROBE=1, which this module sets before the first hook runs).
 *
 * Each hooked function carries the CRC of its expected code bytes. The overlay region holds different code at different times
 * (menus, other characters), so a filter that finds other bytes at its address declines (the original runs) and counts a
 * guard reject. See ../PHASE2_DESIGN.md section 10.
 *
 * One address may carry several hooks, one per expected code CRC (every character loads at the same slot addresses): the runtime
 * filter dispatches to the hook whose CRC matches the live bytes. Variants also carry a module match callback, so identical leaf code in different modules stays separate. */
#ifndef JOJO_OVL_H
#define JOJO_OVL_H
#include <stdint.h>

struct CPUState;
typedef int (*JojoOvlFilter)(struct CPUState *cpu, uint32_t addr);

/* Registers a hook when `active` is non-zero (the generated glue decides from JOJO_GAME / JOJO_DIFF / JOJO_FUZZ). */
void jojo_ovl_register(uint32_t addr, uint32_t size, uint32_t crc, JojoOvlFilter filter, int active);
void jojo_ovl_register_variant(uint32_t addr, uint32_t size, uint32_t crc, JojoOvlFilter filter, int active, int (*matches)(void));
/* Bounds-checked code fingerprint, shared by live dispatch and replay. */
int jojo_ovl_matches(uint32_t addr, uint32_t size, uint32_t crc);

/* Called first in every filter. Returns 0 when the filter must decline: this entry is the reference run's own (one-shot skip),
 * or the live bytes at [addr, addr+size) are not the expected code (counted as guard_reject). Counts the entry. */
int jojo_ovl_enter(struct CPUState *cpu, uint32_t addr, uint32_t size, uint32_t crc);

/* Reference run for harness and fuzz: dispatches the original overlay code for one call (interpreter or compiled overlay), with the
 * hook skipped exactly once for this address. Runs to return like jojo_run_to_return. */
void jojo_ovl_ref(struct CPUState *cpu, uint32_t addr);

/* Control binding for the native side of the harness; returns 1 when it ran (then the native body must not). */
int jojo_ovl_control(struct CPUState *cpu, uint32_t addr);

/* Controls for exe glue (JOJO_DIFF_CONTROL): runs `ref` and plants the control change; returns 1 when it ran. */
int jojo_exe_control(struct CPUState *cpu, void (*ref)(struct CPUState *));

#endif
