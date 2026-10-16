/* Capture and replay of real entry states (development only). See jojo_replay.h and ../PHASE2_DESIGN.md section 11.
 *
 * File format: a stream of records, each "magic, compressed length, raw length, zlib payload".
 *   'JRK1' key record: the full RAM image (JR_RAM bytes). Later delta records refer to the latest key record.
 *   'JRD1' entry state: JrHdr, scratchpad, a 512-bit map of the 4 KB RAM pages that differ from the key image, then those pages.
 * A new key record is written when more than JR_REKEY pages differ, so the stream stays small without a fixed base.
 *
 * Sampling (capture): per function at most JOJO_CAPTURE_K states (default 12), at least JOJO_CAPTURE_GAP frames apart (default 120),
 * and after each gap a pseudo-random 0..3 further calls are skipped, so a function called for both fighters is seen for both.
 * JOJO_CAPTURE_MAX_MB (default 1500) caps the file size. JOJO_CAPTURE_SKIP=@<file> lists functions that need no more states.
 *
 * Replay switches: JOJO_REPLAY_FRAME (start frame), JOJO_REPLAY_START (first record index to compare, for restarts),
 * JOJO_REPLAY_SKIP (hex addresses, comma list: not compared), JOJO_REPLAY_ONLY (hex address: only this function),
 * JOJO_REPLAY_PROGRESS (file: "<index> <addr>" written before each compare, so a runner can restart past a hang or abort). */
#include "jojo_replay.h"
#include "jojo_fuzz.h"
#include "jojo_core.h"
#include "cpu_state.h"
#include "psx_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

extern uint8_t *memory_get_ram_ptr(void);
extern uint8_t *memory_get_scratchpad_ptr(void);
extern uint32_t i_stat, i_mask;
extern uint32_t g_dirty_ram_code_gen;
extern void overlay_watch_invalidate_after_ram_restore(void);
extern void starvation_watchdog_heartbeat(void);
extern void freeze_heartbeat_set_paused(int paused);
#ifdef JOJO_SANCOV
extern int jojo_sancov_guided_take(uint32_t addr);   /* build-san only (hybrid/jojo_sancov.c) */
extern void jojo_sancov_hook(uint32_t addr);
#define JR_GUIDED 1
#else
static int jojo_sancov_guided_take(uint32_t addr) { (void)addr; return 0; }
static void jojo_sancov_hook(uint32_t addr) { (void)addr; }
#define JR_GUIDED 0
#endif

#ifdef JOJO_COVERAGE
/* Coverage build (CMake JOJO_COVERAGE): a forked compare copy starts from zeroed counters and, only when its state compared clean,
 * merges them into JOJO_COV_FILE (an LLVM profile pattern, for example <run>/cov-%4m.profraw). So the profile holds exactly the
 * native code paths that ran on states where native and reference agreed. */
extern void __llvm_profile_reset_counters(void) __attribute__((weak));
extern int __llvm_profile_write_file(void) __attribute__((weak));
extern void __llvm_profile_set_filename(const char *) __attribute__((weak));
static void jr_cov_child_start(void) { if (__llvm_profile_reset_counters) __llvm_profile_reset_counters(); }
static void jr_cov_child_end(const JojoFuzzCounts *r) {
    const char *f = getenv("JOJO_COV_FILE");
    if (!f || !*f || !__llvm_profile_write_file || r->compared == 0 || r->mismatches) return;
    __llvm_profile_set_filename(f);
    __llvm_profile_write_file();
}
#else
static void jr_cov_child_start(void) {}
static void jr_cov_child_end(const JojoFuzzCounts *r) { (void)r; }
#endif

#define JR_RAM    (2u * 1024u * 1024u)
#define JR_PAGE   4096u
#define JR_PAGES  (JR_RAM / JR_PAGE)
#define JR_SPAD   1024u
#define JR_REKEY  160u
#define JR_KEY    0x314B524Au   /* "JRK1" */
#define JR_DELTA  0x3144524Au   /* "JRD1" */
#define JR_SLOTS  16384u

typedef struct {
    uint32_t addr, frame, index, ram_len;
    uint32_t gpr[32], pc, hi, lo, cop0[32], gte_data[32], gte_ctrl[32], i_stat, i_mask;
} JrHdr;

#define JR_RAW_MAX (sizeof(JrHdr) + JR_SPAD + JR_PAGES / 8u + JR_RAM)

static uint8_t  g_key[JR_RAM];            /* latest key image (capture and replay) */
static uint8_t  g_raw[JR_RAW_MAX];
static uint8_t *g_z;                      /* compression buffer */
static uLong    g_zcap;

static int jr_flag(const char *name) {
    const char *e = getenv(name);
    return e && *e;
}
int jojo_capture_on(void) { static int v = -1; if (v < 0) v = jr_flag("JOJO_CAPTURE"); return v; }
int jojo_capture_live(void) { static int v = -1; if (v < 0) v = jr_flag("JOJO_CAPTURE_LIVE"); return v; }
int jojo_replay_on(void)  { static int v = -1; if (v < 0) v = jr_flag("JOJO_REPLAY"); return v; }

/* ---- capture ---------------------------------------------------------------------------------------------------------------- */

static struct { uint32_t addr, count, skip, preset, first, last; uint64_t next_frame; } g_slot[JR_SLOTS];   /* first/last: frames of the captured states */   /* preset: count given by JOJO_CAPTURE_SKIP */
static FILE    *g_cf;
static int      g_cinit, g_have_key, g_full;
static uint32_t g_k = 12, g_gap = 120, g_written, g_nkeys;
static uint64_t g_bytes, g_max_bytes = 1500ull << 20, g_from;   /* g_from: JOJO_CAPTURE_FROM, no states before this frame */
static char    *g_only;                   /* JOJO_CAPTURE_ONLY (comma list): capture only these functions */

static char    *g_have_enough;            /* JOJO_CAPTURE_SKIP list (comma separated) */
static int jr_listed(const char *list, uint32_t addr);
static uint32_t jr_hash(uint32_t x) { x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16; return x; }

static void jr_capture_summary(void) {
    uint32_t i, fns = 0;
    for (i = 0; i < JR_SLOTS; ++i) if (g_slot[i].addr && g_slot[i].count > g_slot[i].preset) ++fns;
    fprintf(stderr, "JOJO_CAPTURE summary: states=%u functions=%u key_records=%u bytes=%llu%s\n", g_written, fns, g_nkeys,
            (unsigned long long)g_bytes, g_full ? " (stopped at JOJO_CAPTURE_MAX_MB)" : "");
    for (i = 0; i < JR_SLOTS; ++i)
        if (g_slot[i].addr) fprintf(stderr, "JOJO_CAPTURE fn=0x%08X states=%u frames=%u..%u%s\n", g_slot[i].addr, g_slot[i].count - g_slot[i].preset,
                                    g_slot[i].first, g_slot[i].last, g_slot[i].preset ? " (reached; skipped by JOJO_CAPTURE_SKIP)" : "");
    if (g_cf) fclose(g_cf);
}

static int jr_write(uint32_t magic, const uint8_t *raw, uint32_t len) {
    uLong zl = compressBound(len);
    uint32_t h[3];
    if (zl > g_zcap) { free(g_z); g_z = (uint8_t *)malloc(zl); g_zcap = g_z ? zl : 0; if (!g_z) return 0; }
    if (compress2(g_z, &zl, raw, len, 1) != Z_OK) return 0;
    h[0] = magic; h[1] = (uint32_t)zl; h[2] = len;
    if (fwrite(h, sizeof h, 1, g_cf) != 1 || fwrite(g_z, 1, zl, g_cf) != zl) return 0;
    g_bytes += sizeof h + zl;
    return 1;
}

void jojo_capture_entry(CPUState *cpu, uint32_t addr) {
    uint32_t s, i, npages = 0, ram_len = psx_ram_live_bytes();
    uint64_t frame = jojo_frame_now();
    const uint8_t *ram = memory_get_ram_ptr();
    uint8_t *map, *p;
    JrHdr h;
    const char *e;
    if (!g_cinit) {
        g_cinit = 1;
        g_cf = fopen(getenv("JOJO_CAPTURE"), "wb");
        if (!g_cf) { fprintf(stderr, "JOJO_CAPTURE: cannot open %s\n", getenv("JOJO_CAPTURE")); g_full = 1; return; }
        if ((e = getenv("JOJO_CAPTURE_K")) && atoi(e) > 0) g_k = (uint32_t)atoi(e);
        if ((e = getenv("JOJO_CAPTURE_GAP")) && atoi(e) >= 0) g_gap = (uint32_t)atoi(e);
        if ((e = getenv("JOJO_CAPTURE_MAX_MB")) && atoi(e) > 0) g_max_bytes = (uint64_t)atoi(e) << 20;
        if ((e = getenv("JOJO_CAPTURE_FROM")) && atoi(e) > 0) g_from = (uint64_t)atoi(e);
        if ((e = getenv("JOJO_CAPTURE_ONLY")) && *e) g_only = strdup(e);
        if ((e = getenv("JOJO_CAPTURE_SKIP")) && *e == '@') {   /* @file: hex addresses that need no more states */
            FILE *sf = fopen(e + 1, "rb");
            if (sf) {
                long n; fseek(sf, 0, SEEK_END); n = ftell(sf); fseek(sf, 0, SEEK_SET);
                g_have_enough = (char *)calloc((size_t)n + 1, 1);
                if (g_have_enough && fread(g_have_enough, 1, (size_t)n, sf) != (size_t)n) g_have_enough[0] = 0;
                fclose(sf);
                for (char *q = g_have_enough; q && *q; ++q) if (*q == '\n' || *q == '\r' || *q == '\t') *q = ',';
            }
        }
        atexit(jr_capture_summary);
    }
    if (g_full || frame < g_from || (g_only && !jr_listed(g_only, addr))) return;
    {   /* JOJO_CAPTURE_IF=<hex RAM address>: capture only while that byte is non-zero (a rare game mode, e.g. the hit-mode byte
         * 0x80066CA2 during a super's freeze, which the uniform samples miss). */
        static int ld; static uint32_t ifa;
        if (!ld) { const char *v = getenv("JOJO_CAPTURE_IF"); ld = 1; ifa = v ? (uint32_t)strtoul(v, NULL, 16) : 0; }
        if (ifa && memory_get_ram_ptr()[ifa & 0x1FFFFFu] == 0) return;
    }
    if (JR_GUIDED) jojo_sancov_hook(addr);   /* counts this hooked call; tags its game C frame (jojo_sancov.c) */
    for (s = jr_hash(addr) % JR_SLOTS; g_slot[s].addr && g_slot[s].addr != addr; s = (s + 1u) % JR_SLOTS) ;
    if (!g_slot[s].addr) {
        g_slot[s].addr = addr; g_slot[s].skip = jr_hash(addr) & 3u;
        if (g_have_enough && jr_listed(g_have_enough, addr)) { g_slot[s].count = g_k; g_slot[s].preset = g_k; }   /* JOJO_CAPTURE_SKIP: enough states already */
    }
    if (JR_GUIDED && getenv("JOJO_CAPTURE_CALLS")) {   /* coverage-guided: exactly the listed calls (jojo_sancov.c) */
        if (!jojo_sancov_guided_take(addr)) return;
    } else {
        if (g_slot[s].count >= g_k || frame < g_slot[s].next_frame) return;
        if (g_slot[s].skip) { --g_slot[s].skip; return; }
    }
    if (ram_len > JR_RAM) ram_len = JR_RAM;

    /* Pages that differ from the key image; a fresh key record when there are too many. */
    if (g_have_key)
        for (i = 0; i < JR_PAGES; ++i) if (memcmp(ram + i * JR_PAGE, g_key + i * JR_PAGE, JR_PAGE) != 0) ++npages;
    if (!g_have_key || npages > JR_REKEY) {
        memcpy(g_key, ram, JR_RAM);
        if (!jr_write(JR_KEY, g_key, JR_RAM)) { g_full = 1; return; }
        g_have_key = 1; ++g_nkeys; npages = 0;
    }

    memset(&h, 0, sizeof h);
    h.addr = addr; h.frame = (uint32_t)frame; h.index = g_written; h.ram_len = ram_len;
    memcpy(h.gpr, cpu->gpr, sizeof h.gpr);
    h.pc = cpu->pc; h.hi = cpu->hi; h.lo = cpu->lo;
    memcpy(h.cop0, cpu->cop0, sizeof h.cop0);
    memcpy(h.gte_data, cpu->gte_data, sizeof h.gte_data);
    memcpy(h.gte_ctrl, cpu->gte_ctrl, sizeof h.gte_ctrl);
    h.i_stat = i_stat; h.i_mask = i_mask;
    p = g_raw;
    memcpy(p, &h, sizeof h); p += sizeof h;
    memcpy(p, memory_get_scratchpad_ptr(), JR_SPAD); p += JR_SPAD;
    map = p; memset(map, 0, JR_PAGES / 8u); p += JR_PAGES / 8u;
    for (i = 0; i < JR_PAGES; ++i)
        if (memcmp(ram + i * JR_PAGE, g_key + i * JR_PAGE, JR_PAGE) != 0) {
            map[i >> 3] |= (uint8_t)(1u << (i & 7u));
            memcpy(p, ram + i * JR_PAGE, JR_PAGE); p += JR_PAGE;
        }
    if (!jr_write(JR_DELTA, g_raw, (uint32_t)(p - g_raw))) { g_full = 1; return; }
    ++g_written;
    if (g_slot[s].count == g_slot[s].preset) g_slot[s].first = (uint32_t)frame;
    g_slot[s].last = (uint32_t)frame;
    ++g_slot[s].count;
    g_slot[s].next_frame = frame + g_gap;
    g_slot[s].skip = jr_hash(addr ^ (g_slot[s].count * 0x9E3779B9u)) & 3u;
    if (g_bytes >= g_max_bytes) { g_full = 1; fflush(g_cf); }
}

/* ---- replay ----------------------------------------------------------------------------------------------------------------- */

typedef struct { uint32_t addr; const char *group; uint64_t states, blocked, retried, unattempted; JojoFuzzCounts c; } JrStat;
static JrStat   g_st[JR_SLOTS];
static uint32_t g_nst;

static JrStat *jr_stat(uint32_t addr, const char *group) {
    uint32_t i;
    for (i = 0; i < g_nst; ++i) if (g_st[i].addr == addr && strcmp(g_st[i].group, group) == 0) return &g_st[i];
    if (g_nst >= JR_SLOTS) { fprintf(stderr, "JOJO_REPLAY: stats table full\n"); abort(); }
    memset(&g_st[g_nst], 0, sizeof g_st[g_nst]);
    g_st[g_nst].addr = addr;
    g_st[g_nst].group = group;
    return &g_st[g_nst++];
}

static int jr_listed(const char *list, uint32_t addr) {
    char buf[16];
    const char *p = list;
    while (p && *p) {
        size_t n = strcspn(p, ", ");
        if (n && n < sizeof buf) { memcpy(buf, p, n); buf[n] = 0; if ((uint32_t)strtoul(buf, NULL, 16) == addr) return 1; }
        p += n;
        while (*p == ',' || *p == ' ') ++p;
    }
    return 0;
}

static int jr_read(FILE *f, uint32_t *magic, uint32_t *rawlen) {
    uint32_t h[3];
    uLongf dl;
    if (fread(h, sizeof h, 1, f) != 1) return 0;
    if (h[2] > JR_RAW_MAX || (h[0] != JR_KEY && h[0] != JR_DELTA)) { fprintf(stderr, "JOJO_REPLAY: bad record\n"); return 0; }
    if (h[1] > g_zcap) { free(g_z); g_z = (uint8_t *)malloc(h[1]); g_zcap = g_z ? h[1] : 0; if (!g_z) return 0; }
    if (fread(g_z, 1, h[1], f) != h[1]) return 0;
    dl = h[2];
    if (uncompress(g_raw, &dl, g_z, h[1]) != Z_OK || dl != h[2]) { fprintf(stderr, "JOJO_REPLAY: corrupt record\n"); return 0; }
    *magic = h[0]; *rawlen = h[2];
    return 1;
}

/* Loads one state record (in g_raw) into the machine. */
static void jr_load(CPUState *cpu, const CPUState *live, const JrHdr *h) {
    const uint8_t *map, *p;
    uint8_t *ram = memory_get_ram_ptr();
    uint32_t i;
    p = g_raw + sizeof *h;
    memcpy(memory_get_scratchpad_ptr(), p, JR_SPAD); p += JR_SPAD;
    map = p; p += JR_PAGES / 8u;
    memcpy(ram, g_key, JR_RAM);
    for (i = 0; i < JR_PAGES; ++i)
        if (map[i >> 3] & (1u << (i & 7u))) { memcpy(ram + i * JR_PAGE, p, JR_PAGE); p += JR_PAGE; }
    overlay_watch_invalidate_after_ram_restore();
    g_dirty_ram_code_gen++;
    memcpy(cpu, live, sizeof *cpu);
    memcpy(cpu->gpr, h->gpr, sizeof h->gpr);
    cpu->pc = h->pc; cpu->hi = h->hi; cpu->lo = h->lo;
    memcpy(cpu->cop0, h->cop0, sizeof h->cop0);
    memcpy(cpu->gte_data, h->gte_data, sizeof h->gte_data);
    memcpy(cpu->gte_ctrl, h->gte_ctrl, sizeof h->gte_ctrl);
    i_stat = h->i_stat; i_mask = h->i_mask;
    {   /* JOJO_REPLAY_POKE="ADDR=BYTE,..." (hex): planted-condition replay. Each state is replayed with these RAM bytes set, so a
         * branch that live play takes only under a condition the captures never hold (a CD status word, the hit-mode byte) is
         * compared on otherwise real states. Reference and native both start from the planted state. */
        const char *e = getenv("JOJO_REPLAY_POKE");
        while (e && *e) {
            char *end; uint32_t a = (uint32_t)strtoul(e, &end, 16);
            if (*end != '=') break;
            uint32_t v = (uint32_t)strtoul(end + 1, &end, 16);
            ram[a & 0x1FFFFFu] = (uint8_t)v;
            e = *end == ',' ? end + 1 : end;
        }
    }
}

static int g_last_status, g_last_timeout, g_last_ok;   /* how the last failed isolated copy ended */

/* Isolated compare (JOJO_REPLAY_FORK, default on): a forked copy of the process loads the state and compares; the parent keeps
 * its own machine untouched. A hang (the harness alarm, JOJO_REPLAY_ALARM seconds, default 3) or a crash ends only the copy, and
 * the state is counted as blocked. Returns 1 when the copy reported its counts. */
static int jr_compare_isolated(CPUState *cpu, const JojoFuzzFn *fn, JojoFuzzCounts *c, int alarm_s) {
    int fd[2], status = 0, got = 0;
    pid_t pid;
    JojoFuzzCounts r;
    struct pollfd pf;
    if (pipe(fd) != 0) return 0;
    fflush(NULL);
    pid = fork();
    if (pid < 0) { close(fd[0]); close(fd[1]); return 0; }
    if (pid == 0) {
        close(fd[0]);
        /* A copy that crashes or hangs would write the runtime's fatal freeze dump (~115 MB, relative path, once per process,
         * so once per blocked state: 30 to 45 per full replay filled the disk twice). Its other outputs use absolute paths
         * (the pipe, JOJO_DIFF_DIR, JOJO_COV_FILE), so a read-only working directory only stops that dump. */
        if (chdir("/var/empty") != 0) { /* keep going: the dump is only disk use */ }
        memset(&r, 0, sizeof r);
        jr_cov_child_start();
        jojo_diff_set_isolated(alarm_s, c->mismatches > 0);
        jojo_diff_fuzz_call(cpu, fn->addr, fn->size, fn->outs, fn->ref, fn->native, &r);
        jr_cov_child_end(&r);
        if (write(fd[1], &r, sizeof r) != (ssize_t)sizeof r) _exit(3);
        _exit(0);
    }
    close(fd[1]);
    pf.fd = fd[0]; pf.events = POLLIN;
    if (poll(&pf, 1, (alarm_s * 2 + 5) * 1000) > 0 && read(fd[0], &r, sizeof r) == (ssize_t)sizeof r) got = 1;
    close(fd[0]);
    if (!got) kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    g_last_ok = got;
    if (!got) {
        g_last_status = status;
        g_last_timeout = !(WIFEXITED(status) || (WIFSIGNALED(status) && WTERMSIG(status) != SIGKILL));
        return 0;
    }
    c->iters += r.iters; c->compared += r.compared; c->mismatches += r.mismatches; c->reference_only += r.reference_only;
    c->frozen_skip += r.frozen_skip; c->info_undeclared += r.info_undeclared;
    c->cyc_ref += r.cyc_ref; c->cyc_nat += r.cyc_nat; c->mem_ops += r.mem_ops;
    if (r.max_stack_depth > c->max_stack_depth) c->max_stack_depth = r.max_stack_depth;
    return 1;
}

void jojo_replay_maybe(CPUState *cpu) {
    static int s_done;
    static CPUState s_live;
    const char *e, *skip, *only, *prog;
    uint64_t start_frame = 0, first = 0, index = 0;
    uint32_t magic, rawlen, i;
    uint64_t tot_states = 0, no_native = 0, skipped = 0;
    FILE *f;
    char *files, *fname, *fsave = NULL;
    int isolate, alarm_s = 3;
    if (s_done || !jojo_replay_on()) return;
    if ((e = getenv("JOJO_REPLAY_FRAME")) != NULL) start_frame = strtoull(e, NULL, 10);
    if (jojo_frame_now() < start_frame) return;
    s_done = 1;
    if ((e = getenv("JOJO_REPLAY_START")) != NULL) first = strtoull(e, NULL, 10);
    skip = getenv("JOJO_REPLAY_SKIP");
    only = getenv("JOJO_REPLAY_ONLY");
    prog = getenv("JOJO_REPLAY_PROGRESS");
    isolate = !((e = getenv("JOJO_REPLAY_FORK")) && strcmp(e, "0") == 0);
    if ((e = getenv("JOJO_REPLAY_ALARM")) != NULL && atoi(e) > 0) alarm_s = atoi(e);
    files = strdup(getenv("JOJO_REPLAY"));   /* one capture file or a comma list (all replayed in this one boot) */
    memcpy(&s_live, cpu, sizeof s_live);
    /* The live game stops here for good (the process exits after the last state), so the runtime's freeze heartbeat would see a
     * frozen frame counter and write a ~115 MB freeze dump per incident (about 30 per full replay, 3-4 GB per run dir). */
    freeze_heartbeat_set_paused(1);
    fprintf(stderr, "JOJO_REPLAY start frame=%llu files=%s first=%llu isolated=%d (REPLAY: captured game states, compared off the live timeline)\n",
            (unsigned long long)jojo_frame_now(), files, (unsigned long long)first, isolate);
    for (fname = strtok_r(files, ",", &fsave); fname; fname = strtok_r(NULL, ",", &fsave)) {
    f = fopen(fname, "rb");
    if (!f) { fprintf(stderr, "JOJO_REPLAY: cannot open %s\n", fname); exit(2); }
    while (jr_read(f, &magic, &rawlen)) {
        JrHdr h;
        const JojoFuzzFn *fn;
        JrStat *st;
        if (magic == JR_KEY) { memcpy(g_key, g_raw, JR_RAM); continue; }
        memcpy(&h, g_raw, sizeof h);
        ++tot_states;
        if (index++ < first) continue;
        if ((index & 63u) == 0) starvation_watchdog_heartbeat();
        if (only && (uint32_t)strtoul(only, NULL, 16) != h.addr) continue;
        if (skip && jr_listed(skip, h.addr)) { ++skipped; continue; }
        /* Overlay addresses are shared by characters and modes. Select only after restoring this record. */
        jr_load(cpu, &s_live, &h);
        fn = jojo_fuzz_find(h.addr);
        if (!fn) { ++no_native; continue; }
        st = jr_stat(h.addr, jojo_fuzz_group(fn));
        if (!st) continue;
        if (prog) { FILE *pf = fopen(prog, "w"); if (pf) { fprintf(pf, "%llu %08X\n", (unsigned long long)(index - 1), h.addr); fclose(pf); } }
        ++st->states;
        /* Opt-in bounded sweep: a known blocking function cannot pass this
         * replay. Keep reading its states, but never count them as compared. */
        if (st->blocked && jr_flag("JOJO_REPLAY_STOP_BLOCKED")) { ++st->unattempted; continue; }
        /* The state is loaded here, in the full process (the restore takes runtime locks other threads may hold); only the
         * compare runs in the forked copy. A copy that hung in the guest exits 87 (harness alarm): blocked. A copy that never
         * answered is a host-side deadlock of the copy, not of the function: the same state is compared here, in process. */
        if (!isolate) jojo_diff_fuzz_call(cpu, fn->addr, fn->size, fn->outs, fn->ref, fn->native, &st->c);
        else if (!jr_compare_isolated(cpu, fn, &st->c, alarm_s) && g_last_timeout) {
            ++st->retried;
            jr_load(cpu, &s_live, &h);
            jojo_diff_fuzz_call(cpu, fn->addr, fn->size, fn->outs, fn->ref, fn->native, &st->c);
        } else if (!g_last_ok) {
            if (!st->blocked)
                fprintf(stderr, "JOJO_REPLAY BLOCKED fn=0x%08X record=%llu (isolated copy: %s %d)\n", h.addr, (unsigned long long)(index - 1),
                        g_last_timeout ? "no answer, killed" : WIFEXITED(g_last_status) ? "exit" : "signal",
                        g_last_timeout ? 0 : WIFEXITED(g_last_status) ? WEXITSTATUS(g_last_status) : WTERMSIG(g_last_status));
            ++st->blocked;
        }
    }
    fclose(f);
    }
    free(files);
    {
        uint64_t cmp = 0, mis = 0, ro = 0, bad = 0;
        for (i = 0; i < g_nst; ++i) {
            JrStat *s = &g_st[i];
            fprintf(stderr, "JOJO_REPLAY fn=0x%08X states=%llu compared=%llu mismatches=%llu reference_only=%llu frozen_skip=%llu "
                            "info_undeclared=%llu max_stack_depth=%u blocked=%llu cyc_ref=%llu cyc_nat=%llu mem_ops=%llu unattempted=%llu group=%s\n", s->addr, (unsigned long long)s->states,
                    (unsigned long long)s->c.compared, (unsigned long long)s->c.mismatches, (unsigned long long)s->c.reference_only,
                    (unsigned long long)s->c.frozen_skip, (unsigned long long)s->c.info_undeclared, s->c.max_stack_depth,
                    (unsigned long long)s->blocked, (unsigned long long)s->c.cyc_ref, (unsigned long long)s->c.cyc_nat, (unsigned long long)s->c.mem_ops, (unsigned long long)s->unattempted, s->group);
            cmp += s->c.compared; mis += s->c.mismatches; ro += s->c.reference_only; bad += s->c.mismatches ? 1u : 0u;
        }
        fprintf(stderr, "JOJO_REPLAY done states=%llu functions=%u compared=%llu mismatches=%llu functions_mismatching=%llu "
                        "reference_only=%llu no_native=%llu skipped=%llu\n", (unsigned long long)tot_states, g_nst,
                (unsigned long long)cmp, (unsigned long long)mis, (unsigned long long)bad, (unsigned long long)ro,
                (unsigned long long)no_native, (unsigned long long)skipped);
    }
    fflush(stderr);
    exit(0);
}
