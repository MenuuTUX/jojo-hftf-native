/* Phase 2 hybrid glue for the animation VM (ps1-port/game/anim_vm.c). Development only; never part of the final target.
 *
 * Switch: JOJO_GAME containing "anim" runs the native VM for unarmed calls. Unset, the translated reference runs.
 * JOJO_DIFF arms the per-call differential harness (hybrid/jojo_diff.c) for the listed addresses; for an armed address the
 * harness compares the native body with the reference (run to return) and keeps the native result. */
#include "jojo_game_glue.h"
#include "anim_vm.h"

extern void jojo_lle_func_800268BC(CPUState *cpu);
extern void jojo_lle_func_80026A04(CPUState *cpu);
extern void jojo_lle_func_80026A60(CPUState *cpu);
extern void jojo_lle_func_80026AFC(CPUState *cpu);

/* Guest ranges (from the shard's "Size: N bytes" comments). */
#define SET_ADDR 0x800268BCu  /* 164 bytes */
#define TICK_ADDR 0x80026A04u /* 92 bytes */
#define ADV_ADDR 0x80026A60u  /* 156 bytes */
#define AFC_ADDR 0x80026AFCu  /* 52 bytes */
#define SET_SIZE 164u
#define TICK_SIZE 92u
#define ADV_SIZE 156u
#define AFC_SIZE 52u

static int game_anim_on(void) {
    static int state = -1;
    if (state < 0) {
        const char *s = getenv("JOJO_GAME");
        state = (s && strstr(s, "anim")) ? 1 : 0;
    }
    return state;
}

#define gm_for jojo_gm_for   /* shared glue, hybrid/jojo_game_glue.h */

/* Native bodies. Arguments in $a0..$a1, result in $v0, return through the saved $ra. */
static void native_set(CPUState *cpu) {
    GuestMem m = gm_for(cpu);
    uint64_t m0 = g_jojo_memops;
    cpu->gpr[2] = (uint32_t)anim_set(&m, cpu->gpr[4], cpu->gpr[5]);
    jojo_native_charge_own(143u, m0);
    jojo_return(cpu);
}
static void native_tick(CPUState *cpu) {
    GuestMem m = gm_for(cpu);
    uint64_t m0 = g_jojo_memops;
    cpu->gpr[2] = (uint32_t)anim_tick(&m, cpu->gpr[4]);
    jojo_native_charge_own(74u, m0);
    jojo_return(cpu);
}
static void native_advance(CPUState *cpu) {
    GuestMem m = gm_for(cpu);
    cpu->gpr[2] = (uint32_t)anim_advance(&m, cpu->gpr[4], cpu->gpr[5]);
    jojo_return(cpu);
}
static void native_afc(CPUState *cpu) {
    GuestMem m = gm_for(cpu);
    cpu->gpr[2] = anim_0x80026AFC(&m, cpu->gpr[4]);
    jojo_return(cpu);
}

/* Reference bodies for the harness: the translated code run to return. */
static void ref_set(CPUState *cpu)     { jojo_run_to_return(cpu, jojo_lle_func_800268BC, SET_ADDR, SET_SIZE); }
static void ref_tick(CPUState *cpu)    { jojo_run_to_return(cpu, jojo_lle_func_80026A04, TICK_ADDR, TICK_SIZE); }
static void ref_advance(CPUState *cpu) { jojo_run_to_return(cpu, jojo_lle_func_80026A60, ADV_ADDR, ADV_SIZE); }
static void ref_afc(CPUState *cpu)     { jojo_run_to_return(cpu, jojo_lle_func_80026AFC, AFC_ADDR, AFC_SIZE); }

/* Wrappers bound through psxrecomp_select_guest_implementation (family jojoanim, ps1-port/JoJoPS1/CMakeLists.txt). */
void func_800268BC(CPUState *cpu) {
    if (jojo_resumed(cpu, SET_ADDR, SET_SIZE)) { jojo_lle_func_800268BC(cpu); return; }
    jojo_note_call(SET_ADDR, cpu->gpr[31]);
    if (jojo_diff_armed(SET_ADDR)) { jojo_diff_call(cpu, SET_ADDR, SET_SIZE, JD_OUT_V0, ref_set, native_set); return; }
    if (!game_anim_on()) { jojo_lle_func_800268BC(cpu); return; }
    native_set(cpu);
}
void func_80026A04(CPUState *cpu) {
    if (jojo_resumed(cpu, TICK_ADDR, TICK_SIZE)) { jojo_lle_func_80026A04(cpu); return; }
    jojo_note_call(TICK_ADDR, cpu->gpr[31]);
    if (jojo_diff_armed(TICK_ADDR)) { jojo_diff_call(cpu, TICK_ADDR, TICK_SIZE, JD_OUT_V0, ref_tick, native_tick); return; }
    if (!game_anim_on()) { jojo_lle_func_80026A04(cpu); return; }
    native_tick(cpu);
}
void func_80026A60(CPUState *cpu) {
    if (jojo_resumed(cpu, ADV_ADDR, ADV_SIZE)) { jojo_lle_func_80026A60(cpu); return; }
    jojo_note_call(ADV_ADDR, cpu->gpr[31]);
    if (jojo_diff_armed(ADV_ADDR)) { jojo_diff_call(cpu, ADV_ADDR, ADV_SIZE, JD_OUT_V0, ref_advance, native_advance); return; }
    if (!game_anim_on()) { jojo_lle_func_80026A60(cpu); return; }
    native_advance(cpu);
}
void func_80026AFC(CPUState *cpu) {
    if (jojo_resumed(cpu, AFC_ADDR, AFC_SIZE)) { jojo_lle_func_80026AFC(cpu); return; }
    jojo_note_call(AFC_ADDR, cpu->gpr[31]);
    if (jojo_diff_armed(AFC_ADDR)) { jojo_diff_call(cpu, AFC_ADDR, AFC_SIZE, JD_OUT_V0, ref_afc, native_afc); return; }
    if (!game_anim_on()) { jojo_lle_func_80026AFC(cpu); return; }
    native_afc(cpu);
}
