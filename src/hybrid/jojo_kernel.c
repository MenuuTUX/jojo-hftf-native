/* Phase 1 kernel adapters (JOJO_KERNEL=1). The seven exe stubs at 0x80035A30..0x80035A90 are thin wrappers over the BIOS:
 *   InitHeap       JUMPOUT A0 table 0x39      OpenTh / CloseTh / ChangeTh   JUMPOUT B0 table 0x0E / 0x0F / 0x10
 *   EnterCriticalSection / ExitCriticalSection  syscall 1 / 2
 *   SetSp (0x80035A90)                          swap_stack: v0 = old sp, sp = a0
 * Natively implemented here: SetSp, EnterCriticalSection, ExitCriticalSection (their contract is fully visible in the register file: IEc in
 * COP0 SR bit 0 and sp). The four BIOS-table entries (InitHeap and the thread calls) keep the original body: their results depend on BIOS
 * heap and thread tables that this adapter does not model, so every call takes the LLE path (see the report for the call counts). */
#include "jojo_guest.h"
#include "jojo_core.h"
#include "cpu_state.h"

#include <stdint.h>
#include <stdio.h>

#define LLE_DECL(addr) extern void jojo_lle_func_##addr(CPUState *cpu)
LLE_DECL(80035A30); LLE_DECL(80035A40); LLE_DECL(80035A50); LLE_DECL(80035A60);
LLE_DECL(80035A70); LLE_DECL(80035A80); LLE_DECL(80035A90);

#define DECLARE_ARGS() \
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], \
             a2 = cpu->gpr[6], a3 = cpu->gpr[7]
#define NOTE(addr) jojo_note_call((addr), cpu->gpr[31])
#define TRACE(addr) jojo_trace_call((addr), a0, a1, a2, a3, cpu->gpr[2])
#define FALLBACK(addr, fn) do { \
    uint64_t start_ = jojo_cycles_now(); fn(cpu); \
    jojo_note_cycles((addr), jojo_cycles_now() - start_); TRACE(addr); return; \
} while (0)
#define DONE(addr) do { TRACE(addr); jojo_return(cpu); } while (0)

/* Guest cycles of the native paths: set from the reference "counts" cycles per call (JOJO_KERNEL_CYCLES_* default 0 until measured). */
#ifndef JOJO_KERNEL_CYCLES_ENTER
#define JOJO_KERNEL_CYCLES_ENTER 0
#endif
#ifndef JOJO_KERNEL_CYCLES_EXIT
#define JOJO_KERNEL_CYCLES_EXIT 0
#endif
#ifndef JOJO_KERNEL_CYCLES_SETSP
#define JOJO_KERNEL_CYCLES_SETSP 0
#endif

/* The BIOS-table entries: no native model (heap image and thread table are BIOS state). Always the original body. */
void func_80035A30(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80035A30);
    FALLBACK(0x80035A30, jojo_lle_func_80035A30);
}

void func_80035A40(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80035A40);
    FALLBACK(0x80035A40, jojo_lle_func_80035A40);
}

/* ---- Native game threads (Phase 2 step 7, JOJO_GAME containing "threads"; see ../PHASE2_DESIGN.md section 12) ----------------
 * The game's three thread records (0x801FC400) are managed by Capcom code, which switches with ChangeTh only: the dispatcher
 * 0x800107A0 enters a thread, and wait (0x80010914), exit (0x80010948) and jump (0x800109F0) return to main. Here ChangeTh is a
 * host fiber switch: each BIOS thread handle gets a host fiber (psx_fiber.h, as the runtime's own thread mirror), started from the
 * context OpenTh wrote into its TCB (registers at +0x08, EPC +0x88, hi +0x8C, lo +0x90, SR +0x94; TCB array at the kernel word
 * 0x110, 0xC0 bytes each). A suspended context keeps its guest registers, SR and the runtime's host-side nesting state (dispatch
 * depth, host-call depth, mod callback context, bail flag) here, and resumes with ChangeTh returning 1, as the BIOS does.
 * CloseTh keeps the original body (TCB bookkeeping in RAM) and retires the handle's fiber; a reopened handle gets a fresh one. */
#include "psx_fiber.h"
#include "mod_runtime.h"
#include "interrupts.h"
#include <stdlib.h>
#include <string.h>

extern int g_psx_dispatch_depth;
extern int g_jojo_charging;
extern uint64_t g_jojo_memops;
extern void psx_check_interrupts_at(CPUState *cpu, uint32_t resume_pc);
extern void psx_sched_set_current_tcb(CPUState *cpu, uint32_t tcb);   /* takes the TCB address */
extern void psx_sched_save_context(CPUState *cpu, uint32_t tcb, uint32_t resume_pc);

#define JT_MAX 16u
typedef struct {
    psx_fiber_t fiber;
    int live;                         /* fiber holds a running or suspended thread of this handle */
    uint32_t gpr[32], hi, lo, sr;
    int dispatch_depth, bail, charging;   /* charging: g_jojo_charging (jojo_game_glue.h) */
    uint64_t memops;                       /* g_jojo_memops: per-thread GuestMem operation count (jojo_game_glue.h cost rollback) */
    unsigned host_call_depth;
    ModFunctionEntryContext mod;
} JtCtx;
static JtCtx g_jt[JT_MAX];
static uint32_t g_jt_cur;            /* handle index running now (0 = main) */
static int g_jt_init;
static uint64_t g_jt_switches, g_jt_created;

static int jt_on(void) {
    static int v = -1;
    if (v < 0) { const char *e = getenv("JOJO_GAME"); v = (e && strstr(e, "threads")) ? 1 : 0; }
    return v;
}

static uint32_t jt_rd32(CPUState *cpu, uint32_t a) { return cpu->read_word(a); }
static uint32_t jt_tcb(CPUState *cpu, uint32_t idx) { return jt_rd32(cpu, 0x110u) + 0xC0u * idx; }

static void jt_save(CPUState *cpu, JtCtx *c) {
    memcpy(c->gpr, cpu->gpr, sizeof c->gpr);
    c->hi = cpu->hi; c->lo = cpu->lo; c->sr = cpu->cop0[12];
    c->dispatch_depth = g_psx_dispatch_depth; c->bail = g_psx_call_bail; c->charging = g_jojo_charging; c->memops = g_jojo_memops;
    c->host_call_depth = psx_snapshot_host_call_depth();
    mod_runtime_function_entry_context_save(&c->mod);
}

static void jt_restore(CPUState *cpu, const JtCtx *c) {
    memcpy(cpu->gpr, c->gpr, sizeof c->gpr);
    cpu->hi = c->hi; cpu->lo = c->lo; cpu->cop0[12] = c->sr;
    g_psx_dispatch_depth = c->dispatch_depth; g_psx_call_bail = c->bail; g_jojo_charging = c->charging; g_jojo_memops = c->memops;
    psx_snapshot_host_call_restore(c->host_call_depth);
    mod_runtime_function_entry_context_restore(&c->mod);
}

static CPUState *g_jt_cpu;

/* A fresh thread: the context OpenTh put into its TCB, then the guest code from its EPC. A thread whose code returns ends here and
 * hands control back to main for good (the BIOS has no such path; counted). */
static void jt_entry(void *arg) {
    uint32_t idx = (uint32_t)(uintptr_t)arg, tcb, i;
    CPUState *cpu = g_jt_cpu;
    tcb = jt_rd32(cpu, 0x110u) + 0xC0u * idx;
    for (i = 1; i < 32; ++i) cpu->gpr[i] = jt_rd32(cpu, tcb + 8u + 4u * i);
    cpu->hi = jt_rd32(cpu, tcb + 0x8Cu); cpu->lo = jt_rd32(cpu, tcb + 0x90u);
    { uint32_t sr = jt_rd32(cpu, tcb + 0x94u);   /* the TCB holds SR as pushed by an exception: start with the RFE pop (IEp -> IEc) */
      cpu->cop0[12] = (sr & 0xFFFFFFC0u) | ((sr >> 2) & 0x0Fu); }
    g_psx_dispatch_depth = 0; g_psx_call_bail = 0; g_jojo_charging = 0;
    psx_snapshot_host_call_restore(0);
    psx_dispatch(cpu, jt_rd32(cpu, tcb + 0x88u));
    fprintf(stderr, "JOJO_THREADS thread %u returned from its entry; back to main\n", idx);
    g_jt[idx].live = 0;
    g_jt_cur = 0;
    psx_sched_set_current_tcb(cpu, jt_tcb(cpu, 0));
    psx_fiber_switch(g_jt[0].fiber);
}

static void jt_summary(void) {
    fprintf(stderr, "JOJO_THREADS native switches=%llu threads_created=%llu\n", (unsigned long long)g_jt_switches, (unsigned long long)g_jt_created);
}

/* ChangeTh(handle): returns 1 in the resumed context. */
static void jt_change(CPUState *cpu, uint32_t handle) {
    uint32_t to = handle & 0xFFFFu, from = g_jt_cur;
    JtCtx *t;
    if (!g_jt_init) {
        g_jt_init = 1;
        g_jt[0].fiber = psx_fiber_convert_thread();
        g_jt[0].live = 1;
        atexit(jt_summary);
    }
    g_jt_cpu = cpu;
    cpu->gpr[2] = 1u;                          /* ChangeTh's result, in every resumed context */
    if (to >= JT_MAX || to == from) return;
    t = &g_jt[to];
    if (!t->live || !t->fiber) {
        if (t->fiber) psx_fiber_destroy(t->fiber);
        t->fiber = psx_fiber_create(1024u * 1024u, jt_entry, (void *)(uintptr_t)to);
        if (!t->fiber) { fprintf(stderr, "JOJO_THREADS: psx_fiber_create failed\n"); abort(); }
        t->live = 1;
        memset(&t->mod, 0, sizeof t->mod);
        ++g_jt_created;
    }
    { static int64_t c = -1; if (c < 0) { const char *e = getenv("JOJO_THREADS_CHANGE_CYCLES"); c = e ? atoll(e) : 0; }
      if (c > 0) jojo_cycles_charge((uint32_t)c); }   /* the BIOS switch's guest cost, measured on the original path */
    jt_save(cpu, &g_jt[from]);
    psx_sched_save_context(cpu, jt_tcb(cpu, from), cpu->gpr[31]);   /* as the BIOS: the yielding context into its TCB */
    ++g_jt_switches;
    g_jt_cur = to;
    psx_sched_set_current_tcb(cpu, jt_tcb(cpu, to));
    psx_fiber_switch(t->fiber);
    /* back in this context */
    jt_restore(cpu, &g_jt[from]);
    cpu->gpr[2] = 1u;
}

void func_80035A50(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80035A50);
    if (!jt_on()) FALLBACK(0x80035A50, jojo_lle_func_80035A50);
    {
        uint32_t idx = a0 & 0xFFFFu;
        jojo_lle_func_80035A50(cpu);           /* BIOS CloseTh: TCB bookkeeping in RAM */
        if (idx && idx < JT_MAX) g_jt[idx].live = 0;   /* its fiber is never resumed; replaced when the handle runs again */
        TRACE(0x80035A50);
    }
}

static uint64_t g_lle_change_calls, g_lle_change_cycles;
static void jt_lle_summary(void) {
    if (g_lle_change_calls)
        fprintf(stderr, "JOJO_THREADS lle ChangeTh calls=%llu guest_cycles=%llu (mean %llu per call)\n", (unsigned long long)g_lle_change_calls,
                (unsigned long long)g_lle_change_cycles, (unsigned long long)(g_lle_change_cycles / g_lle_change_calls));
}

void func_80035A60(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80035A60);
    if (!jt_on()) {
        /* measured for the native path's charge (JOJO_THREADS_CHANGE_CYCLES); the BIOS switch runs across two contexts, so the
         * span is from the call to the return in whichever context resumes */
        uint64_t t0 = jojo_cycles_now();
        if (!g_lle_change_calls) atexit(jt_lle_summary);
        jojo_lle_func_80035A60(cpu);
        ++g_lle_change_calls; g_lle_change_cycles += jojo_cycles_now() - t0;
        TRACE(0x80035A60); return;
    }
    {
        uint32_t ra = cpu->gpr[31];
        jt_change(cpu, a0);
        cpu->gpr[31] = ra;
        /* The runtime's own switch re-dispatches the resumed context at its return address, and dispatch entry takes pending
         * interrupts; do the same here, at the same guest PC. */
        if (!(getenv("JOJO_THREADS_NO_IRQ_CHECK"))) psx_check_interrupts_at(cpu, ra);
        DONE(0x80035A60);
    }
}

/* EnterCriticalSection (syscall 1): returns the previous interrupt-enable bit (SR.IEc) and clears it. */
void func_80035A70(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80035A70);
    if (!jojo_piece_on(JOJO_PIECE_KERNEL)) FALLBACK(0x80035A70, jojo_lle_func_80035A70);
    cpu->gpr[2] = cpu->cop0[12] & 1u;
    cpu->cop0[12] &= ~1u;
    jojo_cycles_charge(JOJO_KERNEL_CYCLES_ENTER); DONE(0x80035A70);
}

/* ExitCriticalSection (syscall 2): sets SR.IEc and IM2 like the original syscall (runtime traps.c case 2), and leaves $v0 alone:
 * SYS(02h) has no result, and the game returns an earlier result through it (0x800108B8 returns OpenTh's handle; setting $v0 to
 * the old IEc made it return 0, found by replay phase2-336). */
void func_80035A80(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80035A80);
    if (!jojo_piece_on(JOJO_PIECE_KERNEL)) FALLBACK(0x80035A80, jojo_lle_func_80035A80);
    cpu->cop0[12] |= 0x0401u;
    jojo_cycles_charge(JOJO_KERNEL_CYCLES_EXIT); DONE(0x80035A80);
}

/* SetSp / swap_stack: v0 = the old sp, sp = a0. The only stack switch the game makes is 0x8001F0A8 (scratchpad top in, then back).
 * Unaligned or zero targets are not modelled natively: they take the original body. */
void func_80035A90(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80035A90);
    if (!jojo_piece_on(JOJO_PIECE_KERNEL) || (a0 & 3u) || a0 == 0u) FALLBACK(0x80035A90, jojo_lle_func_80035A90);
    cpu->gpr[2] = cpu->gpr[29];
    cpu->gpr[29] = a0;
    jojo_cycles_charge(JOJO_KERNEL_CYCLES_SETSP); DONE(0x80035A90);
}
