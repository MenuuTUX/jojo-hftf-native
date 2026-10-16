/* Per-call differential harness (development only; never part of the final target). See ../PHASE2_DESIGN.md.
 *
 * JOJO_DIFF=<hex addr>[,<hex addr>...] arms the listed guest functions. A wrapper for an armed function calls jojo_diff_call(),
 * which runs the reference body and then the native body from the same entry state, compares the results and keeps the native
 * result. Both runs execute inside an uncharged span (psx_cycle_freeze.h): no device is serviced and no interrupt is taken, and
 * the guest clock is put back after each run.
 *
 * Generated bodies are resumable: a call returns to the dispatcher with cpu->pc at the callee, and the body resumes at its
 * continuation through a switch on cpu->pc. A body therefore runs to completion only through jojo_run_to_return(), which
 * dispatches each callee and resumes the body at its continuation. A single call of the body stops at its first call.
 *
 * Other switches:
 *   JOJO_DIFF_EVERY=<n>  compare only calls 1, n+1, 2n+1, ... (the native body still runs on every call)
 *   JOJO_DIFF_DIR=<dir>  where the RAM images of the first mismatch of each function are written (default: current directory)
 */
#include "jojo_diff.h"
#include "jojo_core.h"
#include "cpu_state.h"
#include "interrupts.h"
#include "psx_memory.h"
#include "psx_cycle_freeze.h"

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

extern uint8_t *memory_get_ram_ptr(void);
extern uint8_t *memory_get_scratchpad_ptr(void);
extern uint32_t i_stat, i_mask;   /* INTC state (memory.c): global, outside the entry snapshot */

#define JD_MAX_FN   2048u
#define JD_RAM_MAX  (2u * 1024u * 1024u)
#define JD_SPAD     1024u
#define JD_STACK_BELOW 0x10000u  /* dead stack below the entry sp, 64 KB: the reference saves registers there, native never does.
                                    The measured depth per function is printed in the summary (max over calls). */
#define JD_SPAD_GLOBALS_END 0x60u  /* scratchpad offsets below this hold game globals: never inside the stack window */
#define JD_ARG_HOME 16u          /* caller-allocated argument home area at the entry sp: the reference may write it */
#define JD_ITER_CAP 1000000u     /* run-to-return loop: abort loudly past this many steps */

typedef struct {
    uint32_t addr, size, outs;
    uint64_t calls, compared, mismatches, reference_only, nested, unsampled, frozen_skip, info_undeclared;
    uint64_t cyc_ref, cyc_nat, mem_ops;   /* mem_ops: GuestMem operations of the native run; guest cycles over compared calls: the reference run, and guest code the native run still calls */
    uint32_t max_stack_depth;    /* deepest reference write below entry sp, bytes (over compared calls) */
    int      dumped;             /* first mismatch already written */
} JdFn;

int g_jojo_charging;      /* jojo_game_glue.h jojo_native_enter */
uint64_t g_jojo_memops;   /* GuestMem operations made by native bodies (jojo_game_glue.h) */
static JdFn     g_fn[JD_MAX_FN];
static uint32_t g_nfn;
static int      g_init;
static uint32_t g_every = 1;
static uint64_t g_cap;           /* JOJO_DIFF_CAP=<n>: after n compared calls of a function, compare only every 64th (the rest run native only) */
static int      g_isolated;      /* 1 in a forked compare copy (jojo_diff_set_isolated): the process is discarded afterwards */
static int      g_alarm = 30;    /* seconds before a run under the harness counts as hung (jojo_diff_set_isolated) */
static int      g_nodump;        /* 1: write no RAM images (an isolated copy whose function already has a dump) */
static int      g_fuzz;          /* 1 while the synthetic fuzz driver (jojo_fuzz.c) calls: no clock charge, own counters */
static int      g_depth;         /* > 0 while a reference or native body runs under the harness */
/* Wrapper entries with pc inside the function (continuations), per function. They run without the harness. */
static struct { uint32_t addr; uint64_t n; } g_res[JD_MAX_FN];
static uint32_t g_nres;
static const char *g_dump_dir = ".";

/* Entry state, state after the reference run, and scratch copies; static to keep the 2 MB buffers off the host stack. */
static CPUState e_cpu, r_cpu;
static uint8_t  e_ram[JD_RAM_MAX], r_ram[JD_RAM_MAX];
static uint8_t  e_spad[JD_SPAD], r_spad[JD_SPAD];

static void jd_summary(void);
static volatile uint32_t g_cur_addr;     /* function under the harness, for the hang watchdog */
static volatile int g_cur_phase;
static void jd_hang(int sig) {
    char msg[128];
    int n = snprintf(msg, sizeof msg, "JOJO_DIFF HANG fn=0x%08X phase=%s (no return after the alarm; waits on a frozen clock?)\n", g_cur_addr, g_cur_phase == 1 ? "reference" : "native");
    (void)sig;
    if (n > 0) { ssize_t w = write(2, msg, (size_t)n); (void)w; }
    _exit(87);
}

static void jd_init(void) {
    const char *list, *every;
    if (g_init) return;
    g_init = 1;
    list = getenv("JOJO_DIFF");
    every = getenv("JOJO_DIFF_EVERY");
    if (every && atoi(every) > 0) g_every = (uint32_t)atoi(every);
    if (getenv("JOJO_DIFF_DIR")) g_dump_dir = getenv("JOJO_DIFF_DIR");
    if (list) {
        char *buf = NULL, *tok, *save = NULL;
        size_t len;
        if (list[0] == '@') {               /* JOJO_DIFF=@<file>: hex addresses separated by commas, spaces or newlines */
            FILE *lf = fopen(list + 1, "rb");
            if (lf) {
                fseek(lf, 0, SEEK_END); len = (size_t)ftell(lf); fseek(lf, 0, SEEK_SET);
                buf = (char *)malloc(len + 1);
                if (buf && fread(buf, 1, len, lf) == len) buf[len] = 0; else { free(buf); buf = NULL; }
                fclose(lf);
            } else fprintf(stderr, "JOJO_DIFF: cannot open %s\n", list + 1);
        } else buf = strdup(list);
        if (buf) {
            for (tok = strtok_r(buf, ", \r\n\t", &save); tok; tok = strtok_r(NULL, ", \r\n\t", &save)) {
                if (g_nfn < JD_MAX_FN) {
                    g_fn[g_nfn].addr = (uint32_t)strtoul(tok, NULL, 16);
                    ++g_nfn;
                }
            }
            free(buf);
        }
    }
    if ((every = getenv("JOJO_DIFF_CAP")) != NULL && atoi(every) > 0) g_cap = (uint64_t)atoi(every);
    if (g_nfn) atexit(jd_summary);
}

int jojo_diff_armed(uint32_t addr) {
    uint32_t i;
    jd_init();
    for (i = 0; i < g_nfn; ++i)
        if (g_fn[i].addr == addr) return 1;
    return 0;
}

static JdFn *jd_stat(uint32_t addr) {
    uint32_t i;
    for (i = 0; i < g_nfn; ++i)
        if (g_fn[i].addr == addr) return &g_fn[i];
    return NULL;
}

int jojo_resumed(const CPUState *cpu, uint32_t addr, uint32_t size) {
    uint32_t i;
    if (!(cpu->pc > addr && cpu->pc < addr + size)) return 0;
    for (i = 0; i < g_nres && g_res[i].addr != addr; ++i) ;
    if (i == g_nres) {
        if (g_nres >= JD_MAX_FN) return 1;
        g_res[g_nres].addr = addr;
        g_res[g_nres].n = 0;
        ++g_nres;
    }
    ++g_res[i].n;
    return 1;
}

void jojo_run_to_return(CPUState *cpu, JojoGuestBody body, uint32_t addr, uint32_t size) {
    uint32_t ret = cpu->gpr[31], cont, iter = 0;
    psx_snapshot_host_call_begin();
    body(cpu);
    for (;;) {
        /* pc == 0 means "returned through $ra" (dirty_ram_interp.h): the return target is $ra. */
        if (cpu->pc == 0) cpu->pc = cpu->gpr[31];
        if (cpu->pc == ret) break;
        if (++iter > JD_ITER_CAP) {
            fprintf(stderr, "JOJO run-to-return: iteration cap at pc=0x%08X (body 0x%08X)\n", cpu->pc, addr);
            abort();
        }
        if (cpu->pc >= addr && cpu->pc < addr + size) { body(cpu); continue; }   /* resume at a continuation */
        cont = cpu->gpr[31];
        psx_dispatch_call(cpu, cpu->pc, cont);
        if (cpu->pc == 0) cpu->pc = cpu->gpr[31];
        if (cpu->pc != cont) {
            fprintf(stderr, "JOJO run-to-return: dispatch stopped at pc=0x%08X, expected continuation 0x%08X\n", cpu->pc, cont);
            abort();
        }
    }
    psx_snapshot_host_call_end();
}

/* Runs one body to return inside an uncharged span with interrupts off (as call_guest does for the CD callbacks). The caller
 * checks g_psx_guest_time_frozen first: begin() does nothing while time is already frozen. Returns the cycles executed. */
static uint64_t jd_run(CPUState *cpu, JojoGuestBody body, uint32_t addr, uint32_t size) {
    PsxCycleFreeze save;
    uint32_t sr = cpu->cop0[12];
    uint64_t counted;
    cpu->cop0[12] = sr & ~1u;
    psx_cycle_uncharged_begin(&save, 0);
    ++g_depth;
    jojo_run_to_return(cpu, body, addr, size);
    --g_depth;
    counted = psx_cycle_uncharged_counted();   /* read before end(), which discards it */
    psx_cycle_uncharged_end(&save);
    cpu->cop0[12] = (cpu->cop0[12] & ~1u) | (sr & 1u);
    return counted;
}

static uint32_t jd_phys(uint32_t addr, uint32_t ram_len) {
    return (addr & 0x1fffffffu) % ram_len;
}

static void jd_dump(JdFn *f, uint64_t call, const uint8_t *ref, const uint8_t *nat, uint32_t len) {
    char path[512];
    FILE *fp;
    if (f->dumped || g_nodump) return;
    f->dumped = 1;
    snprintf(path, sizeof path, "%s/jojo_diff_%08X_call%llu_ref.bin", g_dump_dir, f->addr, (unsigned long long)call);
    fp = fopen(path, "wb");
    if (fp) { fwrite(ref, 1, len, fp); fclose(fp); }
    snprintf(path, sizeof path, "%s/jojo_diff_%08X_call%llu_nat.bin", g_dump_dir, f->addr, (unsigned long long)call);
    fp = fopen(path, "wb");
    if (fp) { fwrite(nat, 1, len, fp); fclose(fp); }
    fprintf(stderr, "JOJO_DIFF dumped RAM images: %s/jojo_diff_%08X_call%llu_{ref,nat}.bin\n", g_dump_dir, f->addr,
            (unsigned long long)call);
}

/* Compares one register-sized value: ref against nat. `always` = compared even when the reference did not change it. */
static int jd_cmp_reg(JdFn *f, uint64_t call, uint64_t frame, const char *name,
                      uint32_t entry, uint32_t ref, uint32_t nat, int always) {
    if (!always && ref == entry && nat == entry) return 0;
    if (ref == nat) return 0;
    fprintf(stderr, "JOJO_DIFF MISMATCH fn=0x%08X call=%llu frame=%llu %s ref=0x%08X nat=0x%08X\n",
            f->addr, (unsigned long long)call, (unsigned long long)frame, name, ref, nat);
    return 1;
}

/* An undeclared output (not in the function's outs mask) that differs: counted as info, neither a pass nor a failure. */
static void jd_info_undeclared(JdFn *f, uint64_t call, uint64_t frame, const char *name, uint32_t ref, uint32_t nat) {
    if (ref == nat) return;
    if (f->info_undeclared == 0 && !g_fuzz)
        fprintf(stderr, "JOJO_DIFF INFO undeclared output differs fn=0x%08X call=%llu frame=%llu %s ref=0x%08X nat=0x%08X "
                        "(not compared; see PHASE2_DESIGN.md, declared outputs)\n",
                f->addr, (unsigned long long)call, (unsigned long long)frame, name, ref, nat);
    ++f->info_undeclared;
}

static int jd_read_all(int fd, void *buf, size_t n) {
    uint8_t *p = buf;
    while (n) { ssize_t k = read(fd, p, n); if (k <= 0) return -1; p += k; n -= (size_t)k; }
    return 0;
}
static int jd_write_all(int fd, const void *buf, size_t n) {
    const uint8_t *p = buf;
    while (n) { ssize_t k = write(fd, p, n); if (k <= 0) return -1; p += k; n -= (size_t)k; }
    return 0;
}

void jojo_diff_call(CPUState *cpu, uint32_t addr, uint32_t size, uint32_t outs, JojoGuestBody ref, JojoGuestBody native) {
    JdFn *f = jd_stat(addr);
    uint32_t ram_len, sp_phys, i, j;
    uint64_t call, frame, dev_ref[JOJO_DEV_COUNT], dev_nat[JOJO_DEV_COUNT], dev_base[JOJO_DEV_COUNT];
    int bad = 0, touched = 0, twin_fd = -1;
    pid_t twin_pid = 0;
    uint8_t *ram;
    uint8_t *spad;
    uint32_t sp_entry;
    uint64_t ref_cycles;
    uint32_t e_is, e_im, r_is, r_im;

    if (!f) { native(cpu); return; }
    f->addr = addr;
    f->size = size;
    f->outs = outs;
    call = ++f->calls;
    frame = jojo_frame_now();
    if (g_depth > 0) { ++f->nested; native(cpu); return; }                 /* nested call inside a diffed run: not compared */
    if (g_every > 1 && ((call - 1) % g_every) != 0) { ++f->unsampled; native(cpu); return; }
    if (g_cap && f->compared >= g_cap && (call % 64u) != 0) { ++f->unsampled; native(cpu); return; }   /* hot function: sampled after the cap */
    if (g_psx_guest_time_frozen) { ++f->frozen_skip; native(cpu); return; }  /* already inside an uncharged span: cannot restore clock */

    ram = memory_get_ram_ptr();
    spad = memory_get_scratchpad_ptr();
    ram_len = psx_ram_live_bytes();
    if (ram_len > JD_RAM_MAX) ram_len = JD_RAM_MAX;
    sp_entry = cpu->gpr[29];
    sp_phys = jd_phys(sp_entry, ram_len);

    /* Entry snapshot. */
    memcpy(&e_cpu, cpu, sizeof e_cpu);
    memcpy(e_ram, ram, ram_len);
    memcpy(e_spad, spad, JD_SPAD);
    for (j = 0; j < JOJO_DEV_COUNT; ++j) dev_base[j] = jojo_device_count((JojoDevice)j);
    e_is = i_stat;
    e_im = i_mask;

    /* Twin (forked copy only): a reference run that touches a device perturbs the device state the native run starts from (CD
     * and GPU counters, RAM mirrors), so a native run after it reports false device mismatches. The twin is forked here, with
     * the pristine device state, and waits: when the reference touched a device it receives the reference result and runs the
     * native itself; otherwise it exits unused. The reference-side process then never returns to the caller. */
    if (g_isolated) {
        int tp[2];
        if (pipe(tp) == 0) {
            fflush(NULL);
            pid_t tw = fork();
            if (tw == 0) {
                char go = 'n';
                close(tp[1]);
                if (jd_read_all(tp[0], &go, 1) != 0 || go != 'y') _exit(0);
                if (jd_read_all(tp[0], &r_cpu, sizeof r_cpu) || jd_read_all(tp[0], r_ram, ram_len) ||
                    jd_read_all(tp[0], r_spad, JD_SPAD) || jd_read_all(tp[0], &r_is, sizeof r_is) ||
                    jd_read_all(tp[0], &r_im, sizeof r_im) || jd_read_all(tp[0], dev_ref, sizeof dev_ref) ||
                    jd_read_all(tp[0], &ref_cycles, sizeof ref_cycles)) _exit(4);
                close(tp[0]);
                touched = 1;
                goto from_twin;
            }
            close(tp[0]);
            twin_pid = tw; twin_fd = tp[1];
        }
    }

    /* Reference run (runs to return, through the dispatcher). */
    g_cur_addr = addr; g_cur_phase = 1;
    signal(SIGALRM, jd_hang); alarm((unsigned)g_alarm);
    ref_cycles = jd_run(cpu, ref, addr, size);
    alarm(0);
    for (j = 0; j < JOJO_DEV_COUNT; ++j) dev_ref[j] = jojo_device_count((JojoDevice)j) - dev_base[j];
    r_is = i_stat;
    r_im = i_mask;
    memcpy(&r_cpu, cpu, sizeof r_cpu);
    memcpy(r_ram, ram, ram_len);
    memcpy(r_spad, spad, JD_SPAD);

    /* Option 1 (reference-only): a reference run that touched a device has already done that device write. A native run would
     * repeat it, and the snapshot cannot undo a device write. So keep the reference result (it is live now), skip the native run,
     * and log the call as reference-only. */
    for (j = 0; j < JOJO_DEV_COUNT; ++j) dev_ref[j] = jojo_device_count((JojoDevice)j) - dev_base[j];
    for (j = 0; j < JOJO_DEV_COUNT; ++j) if (dev_ref[j]) touched = 1;
    /* In a forked copy (g_isolated) the whole process is discarded after the compare, so a repeated device write does no harm:
     * the native run goes ahead and its per-device write counts must equal the reference's (below). Step 9: 22 functions were
     * unverified only because some of their states touched a device. */
    if (twin_pid > 0) {
        char go = touched ? 'y' : 'n';
        int st = 0;
        (void)jd_write_all(twin_fd, &go, 1);
        if (touched) {
            (void)(jd_write_all(twin_fd, &r_cpu, sizeof r_cpu) || jd_write_all(twin_fd, r_ram, ram_len) ||
                   jd_write_all(twin_fd, r_spad, JD_SPAD) || jd_write_all(twin_fd, &r_is, sizeof r_is) ||
                   jd_write_all(twin_fd, &r_im, sizeof r_im) || jd_write_all(twin_fd, dev_ref, sizeof dev_ref) ||
                   jd_write_all(twin_fd, &ref_cycles, sizeof ref_cycles));
            close(twin_fd);
            waitpid(twin_pid, &st, 0);   /* the twin reports the counts; this process only waits for it */
            _exit(WIFEXITED(st) ? WEXITSTATUS(st) : 5);
        }
        close(twin_fd);
        waitpid(twin_pid, &st, 0);
        twin_pid = 0;
    }
    if (touched && !g_isolated) {
        ++f->reference_only;
        if (f->reference_only == 1 || g_fuzz)
            fprintf(stderr, "JOJO_DIFF reference-only fn=0x%08X call=%llu frame=%llu: reference run touched a device (gpu %llu spu %llu cd %llu sio %llu memcard %llu); native run skipped\n",
                    addr, (unsigned long long)call, (unsigned long long)frame, (unsigned long long)dev_ref[JOJO_DEV_GPU],
                    (unsigned long long)dev_ref[JOJO_DEV_SPU], (unsigned long long)dev_ref[JOJO_DEV_CD],
                    (unsigned long long)dev_ref[JOJO_DEV_SIO], (unsigned long long)dev_ref[JOJO_DEV_MEMCARD]);
        if (!g_fuzz) jojo_cycles_charge((uint32_t)ref_cycles);
        return;
    }

from_twin:
    /* Dead stack the reference wrote below the entry sp: lowest byte that differs from entry, within the window. */
    {
        uint32_t k, lowest = sp_phys;
        uint32_t floor_ = sp_phys >= JD_STACK_BELOW ? sp_phys - JD_STACK_BELOW : 0;
        if (floor_ < sp_phys && sp_phys <= ram_len && memcmp(r_ram + floor_, e_ram + floor_, sp_phys - floor_) != 0)
            for (k = floor_; k < sp_phys && k < ram_len; ++k)
                if (r_ram[k] != e_ram[k]) { lowest = k; break; }
        if (lowest < sp_phys && sp_phys - lowest > f->max_stack_depth) f->max_stack_depth = sp_phys - lowest;
    }

    /* Native run from the same entry state. */
    memcpy(cpu, &e_cpu, sizeof e_cpu);
    memcpy(ram, e_ram, ram_len);
    memcpy(spad, e_spad, JD_SPAD);
    for (j = 0; j < JOJO_DEV_COUNT; ++j) dev_base[j] = jojo_device_count((JojoDevice)j);
    g_cur_phase = 2; alarm((unsigned)g_alarm);
    { uint64_t m0 = g_jojo_memops; f->cyc_nat += jd_run(cpu, native, addr, size); f->mem_ops += g_jojo_memops - m0; }
    f->cyc_ref += ref_cycles;
    alarm(0);
    for (j = 0; j < JOJO_DEV_COUNT; ++j) dev_nat[j] = jojo_device_count((JojoDevice)j) - dev_base[j];

    /* Device side effects: the native run must make as many writes to each device as the reference (0 unless the reference
     * touched it, which only reaches here in a forked copy). The written values are not compared; GPU commands are built in
     * RAM, which is. */
    ++f->compared;
    for (j = 0; j < JOJO_DEV_COUNT; ++j)
        if (dev_nat[j] != (touched ? dev_ref[j] : 0)) {
            fprintf(stderr, "JOJO_DIFF MISMATCH fn=0x%08X call=%llu frame=%llu device %u writes ref %llu nat %llu\n",
                    addr, (unsigned long long)call, (unsigned long long)frame, j,
                    (unsigned long long)(touched ? dev_ref[j] : 0), (unsigned long long)dev_nat[j]);
            bad = 1;
        }

    /* Outputs: s0-s7, gp, sp, fp, ra and pc always. v0, v1, hi and lo only when the function declares them (outs mask); an
     * undeclared difference is counted as info. Temporaries and argument registers are not compared. */
    {
        static const struct { int idx; const char *name; } saved[] = {
            {16, "s0"}, {17, "s1"}, {18, "s2"}, {19, "s3"}, {20, "s4"}, {21, "s5"}, {22, "s6"}, {23, "s7"},
            {28, "gp"}, {29, "sp"}, {30, "fp"}, {31, "ra"}
        };
        for (j = 0; j < sizeof saved / sizeof saved[0]; ++j)
            bad |= jd_cmp_reg(f, call, frame, saved[j].name, e_cpu.gpr[saved[j].idx], r_cpu.gpr[saved[j].idx],
                              cpu->gpr[saved[j].idx], 1);
        bad |= jd_cmp_reg(f, call, frame, "pc", e_cpu.pc, r_cpu.pc, cpu->pc, 1);
        if (outs & JD_OUT_V0) bad |= jd_cmp_reg(f, call, frame, "v0", e_cpu.gpr[2], r_cpu.gpr[2], cpu->gpr[2], 1);
        else jd_info_undeclared(f, call, frame, "v0", r_cpu.gpr[2], cpu->gpr[2]);
        if (outs & JD_OUT_V1) bad |= jd_cmp_reg(f, call, frame, "v1", e_cpu.gpr[3], r_cpu.gpr[3], cpu->gpr[3], 1);
        else jd_info_undeclared(f, call, frame, "v1", r_cpu.gpr[3], cpu->gpr[3]);
        if (outs & JD_OUT_HILO) {
            bad |= jd_cmp_reg(f, call, frame, "hi", e_cpu.hi, r_cpu.hi, cpu->hi, 0);
            bad |= jd_cmp_reg(f, call, frame, "lo", e_cpu.lo, r_cpu.lo, cpu->lo, 0);
        } else {
            jd_info_undeclared(f, call, frame, "hi", r_cpu.hi, cpu->hi);
            jd_info_undeclared(f, call, frame, "lo", r_cpu.lo, cpu->lo);
        }
        /* COP0 and GTE registers: compared only when the reference changed them. */
        for (j = 0; j < 32; ++j) {
            char name[24];
            snprintf(name, sizeof name, "cop0[%u]", (unsigned)j);
            if (jd_cmp_reg(f, call, frame, name, e_cpu.cop0[j], r_cpu.cop0[j], cpu->cop0[j], 0)) {
                fprintf(stderr, "JOJO_DIFF INTC at %s: entry i_stat=0x%03X i_mask=0x%03X | after ref i_stat=0x%03X i_mask=0x%03X | after native i_stat=0x%03X i_mask=0x%03X\n",
                        name, e_is, e_im, r_is, r_im, i_stat, i_mask);
                bad = 1;
            }
        }
        for (j = 0; j < 32; ++j) {
            char name[24];
            snprintf(name, sizeof name, "gte_data[%u]", (unsigned)j);
            bad |= jd_cmp_reg(f, call, frame, name, e_cpu.gte_data[j], r_cpu.gte_data[j], cpu->gte_data[j], 0);
            snprintf(name, sizeof name, "gte_ctrl[%u]", (unsigned)j);
            bad |= jd_cmp_reg(f, call, frame, name, e_cpu.gte_ctrl[j], r_cpu.gte_ctrl[j], cpu->gte_ctrl[j], 0);
        }
    }

    /* RAM and scratchpad: every byte, except the excluded stack windows (printed once in the summary). */
    {
        int sp_in_spad = (sp_entry & 0xFFFFFC00u) == 0x1F800000u;   /* then no RAM window: the stack is in the scratchpad */
        uint32_t lo_ex = sp_in_spad ? 0 : sp_phys >= JD_STACK_BELOW ? sp_phys - JD_STACK_BELOW : 0;
        uint32_t hi_ex = sp_in_spad ? 0 : sp_phys + JD_ARG_HOME;
        uint32_t diffs = 0, first = 0;
        int have_first = 0;
        for (i = 0; i < ram_len; i += 256u) {           /* equal 256-byte blocks (almost all of RAM) are skipped by memcmp */
            uint32_t end = i + 256u > ram_len ? ram_len : i + 256u, k;
            if (memcmp(r_ram + i, ram + i, end - i) == 0) continue;
            for (k = i; k < end; ++k) {
                if (k >= lo_ex && k < hi_ex) continue;
                /* 80018F3C, and 80017F28 and 80018034 which run its libds call (80049858) copies bytes 5..7 of a stack entry it never initialised into the command queue at
                 * 0x80098BEC, so those three bytes hold whatever the frame held (measured: the native's frame leaves 0x801F, the
                 * reference's 0x0019 or 0). Uninitialised stack content, not an output. */
                if ((addr == 0x80018F3Cu || addr == 0x80017F28u || addr == 0x80018034u) && k >= 0x98BEEu && k < 0x98BF1u) continue;
                if (r_ram[k] != ram[k]) {
                    if (!have_first) { have_first = 1; first = k; }
                    ++diffs;
                }
            }
        }
        if (have_first) {
            fprintf(stderr, "JOJO_DIFF MISMATCH fn=0x%08X call=%llu frame=%llu RAM 0x%08X ref=0x%02X nat=0x%02X (%u bytes differ)\n",
                    addr, (unsigned long long)call, (unsigned long long)frame, 0x80000000u + first,
                    r_ram[first], ram[first], diffs);
            bad = 1;
            if (!f->dumped) jd_dump(f, call, r_ram, ram, ram_len);
        }
        /* The game also runs code with $sp in the scratchpad (SetSp pair 0x80035A90, e.g. 8001F0A8; entry sp 0x1F800300..0x380).
         * Like the RAM rule, the dead stack below the entry sp is left out, here at most 512 bytes and never below scratchpad offset
         * JD_SPAD_GLOBALS_END (the game's scratchpad globals are 0x000..0x05F and 0x380 up: decompile reading), plus the argument home.
         * Every other scratchpad byte is compared (negative control JOJO_DIFF_CONTROL=negspad). The earlier rule (only bytes the
         * reference changed) flagged native-only writes to dead stack: native code reaches guest callees with a different frame. */
        uint32_t spad_lo = 0, spad_hi = 0;
        if (sp_in_spad) {
            uint32_t off = sp_entry & (JD_SPAD - 1u);
            spad_lo = off > 0x200u + JD_SPAD_GLOBALS_END ? off - 0x200u : JD_SPAD_GLOBALS_END;
            if (spad_lo > off) spad_lo = off;
            spad_hi = off + JD_ARG_HOME > JD_SPAD ? JD_SPAD : off + JD_ARG_HOME;
        }
        /* 8001F0A8 moves $sp into the scratchpad itself (SetSp 0x1F800380), though the entry $sp is in RAM: its callee 8001F2E4
         * saves s0/s1 at 0x370/0x374 (dead stack below the new sp) in the MIPS, which the native callees never write (measured:
         * all 48 mismatching states differ only there). */
        if (addr == 0x8001F0A8u) { spad_lo = 0x360; spad_hi = 0x380; }
        for (i = 0; i < JD_SPAD; ++i)
            if (i >= spad_lo && i < spad_hi) continue;
            else if (r_spad[i] != spad[i]) {
                fprintf(stderr, "JOJO_DIFF MISMATCH fn=0x%08X call=%llu frame=%llu SPAD 0x%08X ref=0x%02X nat=0x%02X\n",
                        addr, (unsigned long long)call, (unsigned long long)frame, 0x1f800000u + i, r_spad[i], spad[i]);
                bad = 1;
                break;
            }
    }

    /* Charge the reference's guest cycles to the live clock, so the game keeps the timing of the original body (the native body
     * itself charges nothing). Done on every harness-run call, compared or not. Unsampled and nested calls charge nothing. This
     * runs after the compares: the charge can raise a VBlank or CD edge, which sets CAUSE.IP2 in the live cpu and in the device
     * state, and those are not part of the native body's result. */
    if (!g_fuzz) jojo_cycles_charge((uint32_t)ref_cycles);

    if (bad) ++f->mismatches;
}

static void jd_summary(void) {
    uint32_t i;
    fprintf(stderr, "JOJO_DIFF summary (stack excluded in RAM compare: %u bytes below entry sp, %u-byte argument home at entry sp; "
                    "reference cycles charged to the live clock; reference run to return through the dispatcher)\n",
            JD_STACK_BELOW, JD_ARG_HOME);
    for (i = 0; i < g_nres; ++i)
        fprintf(stderr, "JOJO_DIFF resumptions fn=0x%08X (continuations, run without the harness): %llu\n", g_res[i].addr,
                (unsigned long long)g_res[i].n);
    for (i = 0; i < g_nfn; ++i) {
        JdFn *f = &g_fn[i];
        fprintf(stderr, "JOJO_DIFF fn=0x%08X calls=%llu compared=%llu mismatches=%llu reference_only=%llu nested=%llu "
                        "unsampled=%llu frozen_skip=%llu info_undeclared=%llu max_stack_depth=%u cyc_ref=%llu cyc_nat=%llu mem_ops=%llu%s\n",
                f->addr, (unsigned long long)f->calls, (unsigned long long)f->compared, (unsigned long long)f->mismatches,
                (unsigned long long)f->reference_only, (unsigned long long)f->nested, (unsigned long long)f->unsampled,
                (unsigned long long)f->frozen_skip, (unsigned long long)f->info_undeclared, f->max_stack_depth,
                (unsigned long long)f->cyc_ref, (unsigned long long)f->cyc_nat, (unsigned long long)f->mem_ops,
                f->max_stack_depth >= JD_STACK_BELOW - 0x100u ? " (AT WINDOW EDGE: widen JD_STACK_BELOW)" : "");
    }
}

/* Synthetic fuzz entry (jojo_fuzz.c): one compare of ref and native from a made-up entry state. Counters go to `out`, not to the
 * route counters, and the guest clock is not charged. A function that is not armed is armed for the call only. */
void jojo_diff_fuzz_call(CPUState *cpu, uint32_t addr, uint32_t size, uint32_t outs, JojoGuestBody ref, JojoGuestBody native,
                         JojoFuzzCounts *out) {
    JdFn before, *f;
    int added = 0;
    uint32_t every = g_every;
    jd_init();
    f = jd_stat(addr);
    if (!f) {
        if (g_nfn >= JD_MAX_FN) return;
        memset(&g_fn[g_nfn], 0, sizeof g_fn[g_nfn]);
        g_fn[g_nfn].addr = addr;
        f = &g_fn[g_nfn++];
        added = 1;
    }
    before = *f;
    g_fuzz = 1; g_every = 1;
    jojo_diff_call(cpu, addr, size, outs, ref, native);
    g_fuzz = 0; g_every = every;
    out->iters += 1;
    out->compared += f->compared - before.compared;
    out->mismatches += f->mismatches - before.mismatches;
    out->reference_only += f->reference_only - before.reference_only;
    out->frozen_skip += f->frozen_skip - before.frozen_skip;
    out->info_undeclared += f->info_undeclared - before.info_undeclared;
    out->cyc_ref += f->cyc_ref - before.cyc_ref;
    out->cyc_nat += f->cyc_nat - before.cyc_nat;
    out->mem_ops += f->mem_ops - before.mem_ops;
    if (f->max_stack_depth > out->max_stack_depth) out->max_stack_depth = f->max_stack_depth;
    { int dumped = f->dumped; *f = before; f->dumped = dumped; }
    if (added) --g_nfn;
}

/* Isolated compares (jojo_replay.c, jojo_fuzz.c): a forked copy uses a short hang alarm and may skip the RAM dumps. */
void jojo_diff_set_isolated(int alarm_s, int nodump) {
    jd_init();
    if (alarm_s > 0) g_alarm = alarm_s;
    g_nodump = nodump;
    g_isolated = 1;
}
