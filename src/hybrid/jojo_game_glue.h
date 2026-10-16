/* Shared glue for Phase 2 game groups (development only). Fills the GuestMem interface of game/guest_mem.h from the emulated CPU
 * and gives generated wrappers (tools-phase2/gen_game_glue.py) their helpers. */
#ifndef JOJO_GAME_GLUE_H
#define JOJO_GAME_GLUE_H
#include "jojo_guest.h"
#include "jojo_core.h"
#include "jojo_diff.h"
#include "cpu_state.h"
#include "mod_plugins.h"
#include "guest_mem.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Return address for guest calls made by game code: a fixed interior address of set_anim that no call ever targets. */
#define JOJO_GAME_CALL_RET 0x8002693Cu

/* Exact group tokens: enabling slot-B pl03x must not also enable slot-A pl03. */
static inline int jojo_game_on(const char *group) {
    const char *s = getenv("JOJO_GAME");
    size_t len = strlen(group);
    while (s && *s) {
        s += strspn(s, ", \t\r\n");
        size_t n = strcspn(s, ", \t\r\n");
        if (n == len && memcmp(s, group, n) == 0) return 1;
        s += n;
    }
    return 0;
}

/* 1 when JOJO_LIVE_EXTRA (hex addresses, comma list) names the function: it may run natively under JOJO_GAME although it is not in
 * the bank. For end-to-end checks of functions a per-call replay cannot cover (main loop, thread bodies). */
static inline int jojo_live_extra(uint32_t addr) {
    const char *s = getenv("JOJO_LIVE_EXTRA");
    char buf[12];
    if (!s) return 0;
    snprintf(buf, sizeof buf, "%08X", addr);
    return strstr(s, buf) != NULL;
}

/* JOJO_LIVE_ONLY=1: only the JOJO_LIVE_EXTRA functions run natively (banked ones too are left original), to test them alone. */
/* Native entries of JOJO_LIVE_EXTRA functions, printed at exit ("JOJO_LIVE_EXTRA fn=... entries=..."): end-to-end evidence must
 * show the function really ran natively in the checked run. One table per translation unit (this header is static inline). */
static uint32_t jojo_extra_addr[64], jojo_extra_n[64];
static void jojo_extra_report(void) {
    for (int i = 0; i < 64 && jojo_extra_addr[i]; ++i)
        fprintf(stderr, "JOJO_LIVE_EXTRA fn=0x%08X entries=%u\n", jojo_extra_addr[i], jojo_extra_n[i]);
}
static inline void jojo_extra_count(uint32_t addr) {
    static int armed;
    for (int i = 0; i < 64; ++i) {
        if (jojo_extra_addr[i] == addr) { ++jojo_extra_n[i]; return; }
        if (!jojo_extra_addr[i]) {
            jojo_extra_addr[i] = addr; jojo_extra_n[i] = 1;
            if (!armed) { armed = 1; atexit(jojo_extra_report); }
            return;
        }
    }
}
static inline int jojo_live(int banked, uint32_t addr) {
    static int only = -1;
    if (only < 0) { const char *e = getenv("JOJO_LIVE_ONLY"); only = e && *e == '1'; }
    if (jojo_live_extra(addr)) { jojo_extra_count(addr); return 1; }
    {   /* JOJO_LIVE_OFF (hex list): these run the original even when banked, the baseline of a one-function end-to-end check */
        const char *off = getenv("JOJO_LIVE_OFF");
        char buf[12];
        if (off) { snprintf(buf, sizeof buf, "%08X", addr); if (strstr(off, buf)) return 0; }
    }
    return banked && !only;
}

/* Live play: charge a native body's measured guest cost (game/cycles.tsv, from replay) so loading lag and frame phase stay the
 * original's. Not inside the harness's frozen span. JOJO_NATIVE_CYCLES=0 turns it off. */
extern int g_psx_guest_time_frozen;
/* Guest memory operations made by native bodies (GuestMem reads and writes); a native body's guest cost is modeled as
 * k (16.16 cycles per operation, fitted per function by replay) times its operations, else the mean cycles per call. */
extern uint64_t g_jojo_memops;
extern void *g_jojo_cur_cpu;
extern double jojo_cost_scale(void);
extern void jojo_cost_note(uint32_t mean, uint32_t k16, uint64_t ops, uint64_t charged);
/* Native bodies with a cost entry that are running now (per thread: jojo_kernel.c saves it on a switch). Kept for the kernel save;
 * it no longer suppresses charges. */
extern int g_jojo_charging;
/* Cost of nested native calls (2026-10-14, step 9). Replay measures a native body's cost with every guest-dispatched callee running as
 * guest code (JOJO_GAME is unset there), so the cost (reference cycles minus the guest cycles the native run still executed) covers
 * the body and its direct C callees only. In live play a callee reached through guest dispatch (call4/calli into a hooked native)
 * therefore charges its own cost, and its GuestMem operations are rolled back out of g_jojo_memops when it returns, so the caller's
 * k16 x ops counts only the caller's own operations. The earlier rule (a native call inside a charging native charges nothing)
 * was right only when the callee had been native during the caller's measurement; it lost 80021D94's cost inside 80020F20
 * (phase2-391) and billed 8001FF94/80020B34 at 8001F2E4's rate. Not inside the harness's frozen span: the replay measurement
 * (jojo_diff.c) reads g_jojo_memops around the wrapper. g_jojo_memops is saved per thread (jojo_kernel.c). */
static inline uint64_t jojo_native_enter(uint32_t mean, uint32_t k16) {
    if (mean || k16) ++g_jojo_charging;
    return g_jojo_memops;
}
static inline int jojo_native_cycles_on(void) {
    static int on = -1;
    if (on < 0) { const char *e = getenv("JOJO_NATIVE_CYCLES"); on = !(e && *e == '0'); }
    return on;
}
/* Charges in pieces that stop at each vblank edge and take the interrupt there, so vblank callbacks are not lost on a bulk charge. */
static inline void jojo_native_charge(uint64_t c) {
    CPUState *cpu = (CPUState *)g_jojo_cur_cpu;
    while (c > 0) {
        uint64_t part = c > 0x40000000u ? 0x40000000u : c;
        if (cpu) {   /* stop at each vblank edge and take the interrupt there, as the original does while it runs the body as guest code */
            uint32_t to = interrupts_cycles_to_vblank();
            if (to > 0 && to < part) part = to;
        }
        jojo_cycles_charge((uint32_t)part);
        c -= part;
        if (cpu) psx_check_interrupts_at(cpu, cpu->gpr[31]);
    }
}
/* Development knob: JOJO_COST_OVERRIDE="mean:k16=mean2:k16b;mean:k16=..." replaces a cost pair (the two constants a generated wrapper passes) for timing experiments. Unset: no effect. */
typedef struct { uint32_t m, k, m2, k2; } JojoCostOv;
static inline const JojoCostOv *jojo_cost_ov(int *n) {
    static JojoCostOv t[32]; static int cnt = -1;
    if (cnt < 0) {
        const char *e = getenv("JOJO_COST_OVERRIDE"); cnt = 0;
        while (e && *e && cnt < 32) {
            unsigned a, b, c, d; int used = 0;
            if (sscanf(e, "%u:%u=%u:%u%n", &a, &b, &c, &d, &used) != 4) break;
            t[cnt].m = a; t[cnt].k = b; t[cnt].m2 = c; t[cnt].k2 = d; ++cnt; e += used;
            while (*e == ';') ++e;
        }
    }
    *n = cnt; return t;
}
/* ops = g_jojo_memops - m0 at the return (the caller computes it; descendants have already rolled theirs back). */
static inline void jojo_native_cost(uint32_t mean, uint32_t k16, uint64_t ops) {
    { int n; const JojoCostOv *t = jojo_cost_ov(&n); for (int i = 0; i < n; ++i) if (t[i].m == mean && t[i].k == k16) { mean = t[i].m2; k16 = t[i].k2; break; } }
    if (mean || k16) --g_jojo_charging;
    if (g_psx_guest_time_frozen) return;
    g_jojo_memops -= ops;                    /* back to m0: the caller's count excludes this call */
    if ((int64_t)ops < 0) ops = 0;           /* a thread switch left the counter below m0: charge nothing rather than 2^64 */
    if (!jojo_native_cycles_on()) return;
    { uint64_t c = k16 ? (ops * k16) >> 16 : mean; double sc = jojo_cost_scale(); if (sc != 1.0) c = (uint64_t)((double)c * sc); jojo_cost_note(mean, k16, ops, c); jojo_native_charge(c); }
}
/* Hand-written groups anim, sched, hit: their mean cost (game/cycles_extra.tsv) is their own; m0 = g_jojo_memops at entry. */
static inline void jojo_native_charge_own(uint32_t mean, uint64_t m0) {
    if (g_psx_guest_time_frozen) return;
    g_jojo_memops = m0;
    if (jojo_native_cycles_on() && mean) jojo_cycles_charge(mean);
}

static uint8_t  jg_rd8(void *c, uint32_t a)  { ++g_jojo_memops; return ((CPUState *)c)->read_byte(a); }
static uint16_t jg_rd16(void *c, uint32_t a) { ++g_jojo_memops; return ((CPUState *)c)->read_half(a); }
static uint32_t jg_rd32(void *c, uint32_t a) { ++g_jojo_memops; return ((CPUState *)c)->read_word(a); }
static void jg_wr8(void *c, uint32_t a, uint8_t v)   { ++g_jojo_memops; ((CPUState *)c)->write_byte(a, v); }
static void jg_wr16(void *c, uint32_t a, uint16_t v) { ++g_jojo_memops; ((CPUState *)c)->write_half(a, v); }
static void jg_wr32(void *c, uint32_t a, uint32_t v) { ++g_jojo_memops; ((CPUState *)c)->write_word(a, v); }

/* Guest call from game code, interrupts off for the call (see hybrid/jojo_cd.c call_guest). */
/* Guest calls keep the interrupt state as it is: the original code makes the same calls with interrupts enabled, and callees such
 * as VSync wait for a vblank. (Under the per-call harness time is frozen, so no interrupt is taken there either way.) */
/* psx_mod_call_guest (runtime/src/mod_guest_services.c), step for step, plus a copy of the callee's exit registers for
 * GuestMem.exit_reg. The copy is per translation unit; a native and the glue it calls through share one. */
extern void psx_snapshot_host_call_begin(void);
extern void psx_snapshot_host_call_end(void);
static uint32_t g_jg_exit[32];
static uint32_t jg_guest_call(CPUState *cpu, uint32_t fn, uint32_t ret, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t regs[32];
    const uint32_t pc = cpu->pc, hi = cpu->hi, lo = cpu->lo;
    memcpy(regs, cpu->gpr, sizeof regs);
    cpu->gpr[4] = a0; cpu->gpr[5] = a1; cpu->gpr[6] = a2; cpu->gpr[7] = a3;
    cpu->gpr[31] = ret;
    psx_snapshot_host_call_begin();
    psx_dispatch_call(cpu, fn, ret);
    const uint32_t result = cpu->gpr[2];
    memcpy(g_jg_exit, cpu->gpr, sizeof g_jg_exit);
    memcpy(cpu->gpr, regs, sizeof regs);
    cpu->pc = pc; cpu->hi = hi; cpu->lo = lo;
    psx_snapshot_host_call_end();
    return result;
}
static uint32_t jg_exit_reg(void *c, uint32_t reg) { (void)c; return reg && reg < 32u ? g_jg_exit[reg] : 0u; }
static uint32_t jg_call2(void *c, uint32_t fn, uint32_t a0, uint32_t a1) {
    return jg_guest_call((CPUState *)c, fn, JOJO_GAME_CALL_RET, a0, a1, 0, 0);
}
static uint32_t jg_call4(void *c, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    return jg_guest_call((CPUState *)c, fn, JOJO_GAME_CALL_RET, a0, a1, a2, a3);
}
static uint32_t jg_call4pc(void *c, uint32_t fn, uint32_t return_pc,
                           uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    return jg_guest_call((CPUState *)c, fn, return_pc, a0, a1, a2, a3);
}
static uint32_t jg_gpr_get(void *c, uint32_t reg) {
    return reg < 32u ? ((CPUState *)c)->gpr[reg] : 0u;
}
static void jg_gpr_set(void *c, uint32_t reg, uint32_t value) {
    if (reg && reg < 32u) ((CPUState *)c)->gpr[reg] = value;
}
static uint32_t jg_calli(void *c, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    ((CPUState *)c)->gpr[2] = fn;   /* jalr $v0: the target is in $v0 at entry (psx_mod_call_guest restores the caller's registers) */
    return jg_call4(c, fn, a0, a1, a2, a3);
}
static uint32_t jg_call5(void *c, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    CPUState *cpu = (CPUState *)c;
    uint32_t sp = cpu->gpr[29], result;
    cpu->gpr[29] = sp - 32u;                 /* the caller's frame: outgoing argument slots at sp+0..sp+19 */
    jg_wr32(c, sp - 32u + 16u, a4);
    result = jg_call4(c, fn, a0, a1, a2, a3);
    cpu->gpr[29] = sp;
    return result;
}
static uint32_t jg_sp_get(void *c) { return ((CPUState *)c)->gpr[29]; }
static void jg_sp_set(void *c, uint32_t sp) { ((CPUState *)c)->gpr[29] = sp; }
static uint32_t jg_call1(void *c, uint32_t fn, uint32_t a0) { return jg_call2(c, fn, a0, 0); }

static inline GuestMem jojo_gm_for(CPUState *cpu) {
    GuestMem m;
    g_jojo_cur_cpu = cpu;
    m.ctx = cpu;
    m.rd8 = jg_rd8; m.rd16 = jg_rd16; m.rd32 = jg_rd32;
    m.wr8 = jg_wr8; m.wr16 = jg_wr16; m.wr32 = jg_wr32;
    m.call1 = jg_call1; m.call2 = jg_call2; m.call4 = jg_call4; m.call4pc = jg_call4pc;
    m.gpr_get = jg_gpr_get; m.gpr_set = jg_gpr_set;
    m.calli = jg_calli; m.call5 = jg_call5; m.sp_get = jg_sp_get; m.sp_set = jg_sp_set;
    m.exit_reg = jg_exit_reg;
    return m;
}
#endif
