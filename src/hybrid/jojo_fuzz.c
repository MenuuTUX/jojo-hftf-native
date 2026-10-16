/* Synthetic differential fuzz (development only). Evidence class SYNTHETIC: random entry states are not game states, so a pass
 * here covers code paths the recorded routes never reach, with made-up data. It does not replace the route A/A runs.
 *
 * For each registered function of the named groups: save CPU and RAM, then ITERS times build a random but valid entry state
 * (jojo_diff_fuzz_call compares the translated reference run to return with the native body), then restore CPU and RAM. */
#include "jojo_fuzz.h"
#include "jojo_replay.h"
#include "jojo_core.h"
#include "cpu_state.h"
#include "psx_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

extern uint8_t *memory_get_ram_ptr(void);
extern uint8_t *memory_get_scratchpad_ptr(void);
extern void starvation_watchdog_heartbeat(void);   /* the fuzz is one long wrapper call: keep the watchdog quiet */

#define FZ_RAM (2u * 1024u * 1024u)

typedef struct { const char *name; const JojoFuzzFn *fns; uint32_t n; } FuzzGroup;
static FuzzGroup *g_groups;
static uint32_t g_ngroups;
static int g_done;
static uint64_t g_rng;

void jojo_fuzz_register(const char *group, const JojoFuzzFn *fns, uint32_t n) {
    FuzzGroup *groups = realloc(g_groups, (g_ngroups + 1u) * sizeof *groups);
    if (!groups) { fprintf(stderr, "JOJO_FUZZ: cannot register %s\n", group); abort(); }
    g_groups = groups;
    g_groups[g_ngroups++] = (FuzzGroup){group, fns, n};
}

static uint32_t rnd(void) {   /* xorshift64*, deterministic from JOJO_FUZZ_SEED */
    g_rng ^= g_rng >> 12; g_rng ^= g_rng << 25; g_rng ^= g_rng >> 27;
    return (uint32_t)((g_rng * 0x2545F4914F6CDD1DULL) >> 32);
}

static uint32_t RDW(uint32_t a) { uint8_t *r = memory_get_ram_ptr(); a &= 0x1FFFFFu; return r[a] | (r[a+1] << 8) | (r[a+2] << 16) | ((uint32_t)r[a+3] << 24); }
static void put8(uint32_t a, uint8_t v) { memory_get_ram_ptr()[a & 0x1FFFFFu] = v; }
static void put16(uint32_t a, uint16_t v) { put8(a, (uint8_t)v); put8(a + 1, (uint8_t)(v >> 8)); }
static void put32(uint32_t a, uint32_t v) { put16(a, (uint16_t)v); put16(a + 2, (uint16_t)(v >> 16)); }

/* Animation object (offsets as in game/anim_vm.h and anim_handlers.c). The object and an arena of random bytes sit in RAM away
 * from the guest stack; every pointer field points into the arena at an even address, so reads are valid and, with stores
 * relative to the object, the run stays inside object plus arena (restored afterwards). */
static void setup_anim_obj(CPUState *cpu, const JojoFuzzFn *fn) {
    static const uint32_t cands[] = { 0x80100000u, 0x80140000u, 0x80080000u, 0x800C0000u };
    uint32_t sp = cpu->gpr[29], obj = cands[0], arena, i, s, t;
    for (i = 0; i < sizeof cands / sizeof cands[0]; ++i)
        if (sp < cands[i] - 0x11000u || sp > cands[i] + 0x13000u) { obj = cands[i]; break; }
    arena = obj + 0x1000u;
    for (i = 0; i < 0x12000u; i += 4) put32(obj + i, rnd());
    s = arena + 0x400u + (rnd() % 0x7000u) * 2u;                 /* stream: even, inside the first 64 KB of the arena */
    t = arena + 0x8000u + (rnd() % 0x400u) * 4u;                 /* object table: below 0x9000, clear of the row areas */
    put32(obj + 60, s);
    put32(obj + 72, arena + 0x9000u + (rnd() % 0x1000u) * 4u);
    put32(obj + 300, t);
    put32(t + 4, arena + 0xA000u + (rnd() % 0x800u) * 4u);
    put32(obj + 52, arena + 0x400u + (rnd() % 0x7000u) * 2u);
    if (rnd() & 1u) put8(obj + 42, 0);                           /* both branches of the variable-0 tests */
    if (rnd() & 1u) put8(obj + 77, 0);
    if (rnd() & 1u) put8(s + 3, 0);
    put8(obj + 11, (uint8_t)(rnd() & 1u));
    if (fn->addr == 0x80026BA0u) {   /* record handler: small row index, row byte 9 = 0 so the translated 0x80024328 is never reached */
        static const uint8_t types[] = { 0, 1, 2, 6, 7 };
        uint32_t idx = rnd() % 16u;
        if (rnd() & 1u) put8(obj + 26, types[rnd() % 5u]);
        put32(t + 4, arena + 0xA000u + 0x400u + (rnd() % 0x400u) * 4u);
        put32(obj + 72, arena + 0xA000u + 0x2000u + (rnd() % 0x400u) * 4u);
        for (i = 0; i < 64; ++i) {   /* zero byte 9 of every row either base can reach */
            put8(RDW(t + 4) - 0x100u + i * 16u + 9u, 0);
            put8(RDW(obj + 72) - 0x100u + i * 16u + 9u, 0);
        }
        /* last: the zeroing above can overlap the stream, so the row index is written after it */
        put16(s + 8, (uint16_t)(rnd() & 1u ? idx : (uint16_t)(0u - idx)));   /* negative indexes too: the table sits mid-arena */
    }
    if (fn->addr == 0x80027138u) put8(s + 1, (uint8_t)(rnd() & 0xFCu));   /* jump table must be word aligned after the record */
    cpu->gpr[4] = obj;
}

/* Negative control (JOJO_FUZZ_NEG=1): the native body is followed by one planted RAM change, which the fuzz must report. */
static JojoGuestBody g_neg_inner;
static void neg_native(CPUState *cpu) {
    g_neg_inner(cpu);
    memory_get_ram_ptr()[0x100100u] ^= 1u;
}

/* Fighter objects of the fight (offsets from the Phase 0 report: the fight block starts at 0x800CD268, P1 at +0xAE8, P2 at +0xE78). */
#define FZ_P1 0x800CDD50u
#define FZ_P2 0x800CE0E0u
static void setup_objs(CPUState *cpu, const JojoFuzzFn *fn, uint32_t k) {
    uint32_t i, flip = (k + (rnd() & 1u)) & 1u;
    for (i = 0; i < fn->nargs && i < 4; ++i) {
        uint32_t v;
        if (fn->ptrmask & (1u << i)) { v = ((i & 1u) ^ flip) ? FZ_P2 : FZ_P1; }
        else v = rnd() % 16u;
        cpu->gpr[4 + i] = v;
    }
}

static int fz_listed(const char *list, uint32_t addr) {
    const char *p = list;
    while (*p) {
        char *end;
        unsigned long v;
        while (*p && !((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))) ++p;
        if (!*p) break;
        v = strtoul(p, &end, 16);
        if ((uint32_t)v == addr) return 1;
        p = end;
    }
    return 0;
}

void jojo_fuzz_maybe(CPUState *cpu) {
    const char *want, *e;
    uint32_t iters = 2000, gi, fi, k;
    static uint8_t s_ram[FZ_RAM], s_spad[1024];
    static CPUState s_cpu;
    static uint64_t frames[16];
    static uint32_t nframes, next, parsed;
    static const char *s_want;
    static int s_looked;
    static char *s_list;
    jojo_replay_maybe(cpu);          /* replay mode (jojo_replay.c): runs once, then exits */
    if (g_done) return;
    if (!s_looked) { s_looked = 1; s_want = getenv("JOJO_FUZZ"); }
    want = s_want;
    if (!want) { g_done = 1; return; }
    if (!parsed) {          /* JOJO_FUZZ_FRAME=<f>[,<f>...]: the fuzz runs once at each frame, on the real state there */
        char *buf = strdup((e = getenv("JOJO_FUZZ_FRAME")) ? e : "1000"), *tok, *sv = NULL;
        parsed = 1;
        for (tok = strtok_r(buf, ",", &sv); tok && nframes < 16; tok = strtok_r(NULL, ",", &sv)) frames[nframes++] = strtoull(tok, NULL, 10);
        free(buf);
    }
    if (next >= nframes) { g_done = 1; return; }
    if (jojo_frame_now() < frames[next]) return;
    ++next;
    if ((e = getenv("JOJO_FUZZ_ITERS")) != NULL && atoi(e) > 0) iters = (uint32_t)atoi(e);
    if (!g_rng) g_rng = (e = getenv("JOJO_FUZZ_SEED")) ? strtoull(e, NULL, 10) | 1u : 0x9E3779B97F4A7C15ULL;
    if (!s_list && (e = getenv("JOJO_FUZZ_LIST")) != NULL && *e == '@') {   /* hex addresses, any separators */
        FILE *lf = fopen(e + 1, "rb");
        if (lf) {
            long n; fseek(lf, 0, SEEK_END); n = ftell(lf); fseek(lf, 0, SEEK_SET);
            s_list = (char *)calloc((size_t)n + 1, 1);
            if (s_list && fread(s_list, 1, (size_t)n, lf) != (size_t)n) s_list[0] = 0;
            fclose(lf);
        }
    }
    memcpy(&s_cpu, cpu, sizeof s_cpu);
    memcpy(s_ram, memory_get_ram_ptr(), FZ_RAM);
    memcpy(s_spad, memory_get_scratchpad_ptr(), sizeof s_spad);
    fprintf(stderr, "JOJO_FUZZ start frame=%llu iters=%u groups=%s (SYNTHETIC: random but valid entry states)\n",
            (unsigned long long)jojo_frame_now(), iters, want);
    for (gi = 0; gi < g_ngroups; ++gi) {
        if (!strstr(want, g_groups[gi].name)) continue;
        for (fi = 0; fi < g_groups[gi].n; ++fi) {
            if (g_groups[gi].fns[fi].matches && !g_groups[gi].fns[fi].matches()) continue;
            if ((e = getenv("JOJO_FUZZ_ONLY")) != NULL && g_groups[gi].fns[fi].addr != (uint32_t)strtoul(e, NULL, 16)) continue;
            if (s_list && !fz_listed(s_list, g_groups[gi].fns[fi].addr)) continue;          /* JOJO_FUZZ_LIST=@file */
            if ((e = getenv("JOJO_FUZZ_SKIP")) != NULL && fz_listed(e, g_groups[gi].fns[fi].addr)) continue;
            if ((e = getenv("JOJO_FUZZ_PROGRESS")) != NULL) {   /* the function about to run, so a runner can restart past a hang */
                FILE *pf = fopen(e, "w");
                if (pf) { fprintf(pf, "%08X\n", g_groups[gi].fns[fi].addr); fclose(pf); }
            }
            JojoFuzzFn fz_copy = g_groups[gi].fns[fi];
            const JojoFuzzFn *fn = &fz_copy;
            JojoFuzzCounts c;
            memset(&c, 0, sizeof c);
            if (getenv("JOJO_FUZZ_NEG")) { g_neg_inner = fz_copy.native; fz_copy.native = neg_native; }
            /* Isolated (JOJO_FUZZ_FORK, default on): a forked copy runs all iterations of this function; a hang (alarm 2 s per
             * run) or a crash ends only the copy and the function is reported as blocked. */
            int isolate = !((e = getenv("JOJO_FUZZ_FORK")) && strcmp(e, "0") == 0), fdp[2] = { -1, -1 };
            pid_t pid = 0;
            starvation_watchdog_heartbeat();
            if (isolate && pipe(fdp) == 0) {
                fflush(NULL);
                pid = fork();
                if (pid > 0) {
                    struct pollfd pfd; int st = 0, got = 0;
                    close(fdp[1]);
                    pfd.fd = fdp[0]; pfd.events = POLLIN;
                    if (poll(&pfd, 1, (int)(iters * 4u + 10u) * 1000) > 0 && read(fdp[0], &c, sizeof c) == (ssize_t)sizeof c) got = 1;
                    close(fdp[0]);
                    if (!got) kill(pid, SIGKILL);
                    waitpid(pid, &st, 0);
                    if (!got) { fprintf(stderr, "JOJO_FUZZ BLOCKED fn=0x%08X (hang or crash in the isolated copy)\n", fn->addr); continue; }
                    goto report;
                }
                if (pid == 0) { close(fdp[0]); jojo_diff_set_isolated(2, 0); }
            }
            for (k = 0; k < iters; ++k) {
                if ((k & 63u) == 0) starvation_watchdog_heartbeat();
                memcpy(cpu, &s_cpu, sizeof s_cpu);
                memcpy(memory_get_ram_ptr(), s_ram, FZ_RAM);
                if (fn->kind == JOJO_FUZZ_ANIM_OBJ) setup_anim_obj(cpu, fn);
                else if (fn->kind == JOJO_FUZZ_OBJS) setup_objs(cpu, fn, k);
                cpu->pc = fn->addr;
                cpu->gpr[31] = 0x8002693Cu;     /* return target for the run-to-return loop: never a call target */
                jojo_diff_fuzz_call(cpu, fn->addr, fn->size, fn->outs, fn->ref, fn->native, &c);
            }
            if (pid == 0 && isolate && fdp[1] >= 0) { if (write(fdp[1], &c, sizeof c) != (ssize_t)sizeof c) _exit(3); _exit(0); }
        report:
            fprintf(stderr, "JOJO_FUZZ fn=0x%08X iters=%llu compared=%llu mismatches=%llu reference_only=%llu frozen_skip=%llu "
                            "info_undeclared=%llu max_stack_depth=%u\n", fn->addr, (unsigned long long)c.iters,
                    (unsigned long long)c.compared, (unsigned long long)c.mismatches, (unsigned long long)c.reference_only,
                    (unsigned long long)c.frozen_skip, (unsigned long long)c.info_undeclared, c.max_stack_depth);
        }
    }
    memcpy(cpu, &s_cpu, sizeof s_cpu);
    memcpy(memory_get_ram_ptr(), s_ram, FZ_RAM);
    memcpy(memory_get_scratchpad_ptr(), s_spad, sizeof s_spad);
    fprintf(stderr, "JOJO_FUZZ done frame=%llu\n", (unsigned long long)jojo_frame_now());
}

int jojo_fuzz_wants(const char *group) {
    const char *want = getenv("JOJO_FUZZ");
    if (jojo_capture_on() || jojo_replay_on()) return 1;   /* capture and replay hook every registered function */
    return want && strstr(want, group) != NULL;
}

const JojoFuzzFn *jojo_fuzz_find(uint32_t addr) {
    uint32_t gi, fi;
    for (gi = 0; gi < g_ngroups; ++gi)
        for (fi = 0; fi < g_groups[gi].n; ++fi)
            if (g_groups[gi].fns[fi].addr == addr &&
                (!g_groups[gi].fns[fi].matches || g_groups[gi].fns[fi].matches())) return &g_groups[gi].fns[fi];
    return NULL;
}

const char *jojo_fuzz_group(const JojoFuzzFn *fn) {
    for (uint32_t gi = 0; gi < g_ngroups; ++gi)
        for (uint32_t fi = 0; fi < g_groups[gi].n; ++fi)
            if (&g_groups[gi].fns[fi] == fn) return g_groups[gi].name;
    return "unknown";
}
