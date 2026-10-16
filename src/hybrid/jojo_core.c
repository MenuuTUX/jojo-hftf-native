/* H1 hybrid glue: piece switches, call counts by caller region, device counters, per-frame state recorder, "jojo" debug command.
 * Development only. See jojo_core.h. */
#include "jojo_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "psx_cycles.h"

extern int psx_peek_word(uint32_t addr, uint32_t *out);

/* ---------------------------------------------------------------- piece switches */
static const char *const k_piece_name[JOJO_PIECE_COUNT] = { "pad", "saves", "gte", "math", "cd", "gpu", "audio", "kernel" };
static int g_piece_state = -1;            /* -1 not read yet */
static unsigned g_piece_mask;

const char *jojo_piece_name(JojoPiece p) { return (unsigned)p < JOJO_PIECE_COUNT ? k_piece_name[p] : "?"; }

static void read_pieces(void) {
    g_piece_mask = 0;
    const char *all = getenv("JOJO_PHASE1");
    if (all && *all) {
        if (!strcmp(all, "all")) g_piece_mask = (1u << JOJO_PIECE_COUNT) - 1u;
        else {
            char buf[256]; strncpy(buf, all, sizeof buf - 1); buf[sizeof buf - 1] = 0;
            for (char *t = strtok(buf, ", "); t; t = strtok(NULL, ", "))
                for (int i = 0; i < JOJO_PIECE_COUNT; i++) if (!strcmp(t, k_piece_name[i])) g_piece_mask |= 1u << i;
        }
    }
    for (int i = 0; i < JOJO_PIECE_COUNT; i++) {
        char var[32]; snprintf(var, sizeof var, "JOJO_%s", k_piece_name[i]);
        for (char *c = var; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
        const char *v = getenv(var);
        if (v && *v && strcmp(v, "0")) g_piece_mask |= 1u << i;
    }
    g_piece_state = 1;
    fprintf(stderr, "jojo-core: pieces on:");
    int any = 0;
    for (int i = 0; i < JOJO_PIECE_COUNT; i++) if (g_piece_mask & (1u << i)) { fprintf(stderr, " %s", k_piece_name[i]); any = 1; }
    fprintf(stderr, "%s\n", any ? "" : " none (REFERENCE behavior)");
}

int jojo_piece_on(JojoPiece p) {
    if (g_piece_state < 0) read_pieces();
    return (g_piece_mask >> p) & 1u;
}

/* ---------------------------------------------------------------- call counts by caller region */
#define MAX_FN 256
#define MAX_REGION 24
typedef struct { uint32_t key; uint64_t n; } RegionCount;
typedef struct { uint32_t addr; int nreg; RegionCount reg[MAX_REGION]; uint64_t total; uint64_t cyc_n, cyc_sum, cyc_min, cyc_max; } FnCount;
static FnCount g_fn[MAX_FN]; static int g_nfn;

/* Region key: 0 = exe text (0x80010000..0x80099800), otherwise the 64 KB page of $ra (overlay code lives above the exe text),
 * 1 = kernel/BIOS (below 0x80010000 or in the BIOS ROM), 2 = anything else (unmapped, scratchpad). */
static uint32_t region_of(uint32_t ra) {
    uint32_t a = ra & 0x1FFFFFFFu;
    if (a >= 0x00010000u && a < 0x00099800u) return 0;
    if (a >= 0x00099800u && a < 0x00200000u) return 0x80000000u | (a >> 16);
    if (a < 0x00010000u || (a >= 0x1FC00000u)) return 1;
    return 2;
}

static FnCount *fn_of(uint32_t addr) {
    for (int i = 0; i < g_nfn; i++) if (g_fn[i].addr == addr) return &g_fn[i];
    if (g_nfn >= MAX_FN) return NULL;
    FnCount *f = &g_fn[g_nfn++]; memset(f, 0, sizeof *f); f->addr = addr; f->cyc_min = ~0ull; return f;
}
void *g_jojo_cur_cpu = 0;   /* CPUState* of the wrapper that is running, set by jojo_gm_for (hybrid/jojo_game_glue.h) */
uint64_t jojo_cycles_now(void) { return psx_cycle_count + g_psx_cyc_batch; }
/* Native cost is CPU work, so it takes the CPU charge: at [timing] guest_cycle_scale 1 this is exactly psx_advance_cycles. */
void jojo_cycles_charge(uint32_t cycles) { psx_cpu_charge(cycles); }
/* Development knobs for native cost experiments (hybrid/jojo_game_glue.h jojo_native_cost). JOJO_COST_SCALE=1.05 multiplies every native charge; JOJO_COST_STATS=<file> writes per-cost-pair totals at exit. */
double jojo_cost_scale(void) { static double s = -1.0; if (s < 0.0) { const char *e = getenv("JOJO_COST_SCALE"); s = (e && *e) ? atof(e) : 1.0; if (s <= 0.0) s = 1.0; } return s; }
typedef struct { uint32_t mean, k16; uint64_t calls, ops, charged; } JojoCostStat;
static JojoCostStat g_cost_stat[4096]; static int g_cost_nstat; static int g_cost_stat_on = -1;
static void jojo_cost_stats_dump(void) { const char *p = getenv("JOJO_COST_STATS"); if (!p || !*p) return; FILE *f = fopen(p, "w"); if (!f) return; for (int i = 0; i < g_cost_nstat; ++i) fprintf(f, "%u\t%u\t%llu\t%llu\t%llu\n", g_cost_stat[i].mean, g_cost_stat[i].k16, (unsigned long long)g_cost_stat[i].calls, (unsigned long long)g_cost_stat[i].ops, (unsigned long long)g_cost_stat[i].charged); fclose(f); }
void jojo_cost_note(uint32_t mean, uint32_t k16, uint64_t ops, uint64_t charged) {
    if (g_cost_stat_on < 0) { const char *p = getenv("JOJO_COST_STATS"); g_cost_stat_on = (p && *p) ? 1 : 0; if (g_cost_stat_on) atexit(jojo_cost_stats_dump); }
    if (!g_cost_stat_on) return;
    for (int i = 0; i < g_cost_nstat; ++i) if (g_cost_stat[i].mean == mean && g_cost_stat[i].k16 == k16) { g_cost_stat[i].calls++; g_cost_stat[i].ops += ops; g_cost_stat[i].charged += charged; return; }
    if (g_cost_nstat < 4096) { JojoCostStat *s = &g_cost_stat[g_cost_nstat++]; s->mean = mean; s->k16 = k16; s->calls = 1; s->ops = ops; s->charged = charged; }
}
uint32_t jojo_frame_cycles(uint64_t frame, uint32_t dflt) {
    static int st; static uint32_t *tab; static size_t n; static int konst = -1;
    if (!st) {
        st = 1; const char *e = getenv("JOJO_CYCLE_TABLE");
        if (e && *e) { FILE *f = fopen(e, "rb"); if (f) { fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET); n = (size_t)sz / 4; tab = (uint32_t *)malloc((size_t)sz); if (tab && fread(tab, 4, n, f) != n) n = 0; fclose(f); fprintf(stderr, "jojo-core: cycle table %s, %zu frames\n", e, n); } }
        const char *k = getenv("JOJO_FRAMECYC"); if (k) konst = atoi(k);
    }
    if (tab && frame < n) return tab[frame];
    if (tab) return 0;
    return konst >= 0 ? (uint32_t)konst : dflt;
}

void jojo_note_cycles(uint32_t addr, uint64_t c) {
    FnCount *f = fn_of(addr); if (!f) return;
    f->cyc_n++; f->cyc_sum += c; if (c < f->cyc_min) f->cyc_min = c; if (c > f->cyc_max) f->cyc_max = c;
}

void jojo_note_call(uint32_t addr, uint32_t ra) {
    FnCount *f = fn_of(addr); if (!f) return;
    uint32_t k = region_of(ra); f->total++;
    for (int i = 0; i < f->nreg; i++) if (f->reg[i].key == k) { f->reg[i].n++; return; }
    if (f->nreg < MAX_REGION) { f->reg[f->nreg].key = k; f->reg[f->nreg].n = 1; f->nreg++; }
}

/* ---------------------------------------------------------------- call/return trace */
extern uint64_t s_frame_count;           /* debug server frame counter (vblank count) */
uint64_t jojo_frame_now(void) { return s_frame_count; }
static FILE *g_trace; static int g_trace_state;
static void trace_close(void) { if (g_trace) { fclose(g_trace); g_trace = NULL; } }
void jojo_trace_call(uint32_t addr, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t v0) {
    if (!g_trace_state) {
        g_trace_state = 1; const char *p = getenv("JOJO_TRACE");
        if (p && *p) { g_trace = fopen(p, "w"); if (g_trace) { setvbuf(g_trace, NULL, _IOFBF, 1 << 16); atexit(trace_close); } }
    }
    if (g_trace) fprintf(g_trace, "%llu %08X %08X %08X %08X %08X %08X\n", (unsigned long long)s_frame_count, addr, a0, a1, a2, a3, v0);
}

/* ---------------------------------------------------------------- device counters */
static uint64_t g_dev[JOJO_DEV_COUNT];
static const char *const k_dev_name[JOJO_DEV_COUNT] = { "gpu", "spu", "cd", "sio", "memcard" };
void jojo_note_device(JojoDevice d) { if ((unsigned)d < JOJO_DEV_COUNT) g_dev[d]++; }
uint64_t jojo_device_count(JojoDevice d) { return (unsigned)d < JOJO_DEV_COUNT ? g_dev[d] : 0; }
void jojo_dev_hit(int d) { jojo_note_device((JojoDevice)d); }      /* strong definition of the weak hook in memory.c (device_counters.patch) */

/* ---------------------------------------------------------------- state recorder
 * Binary, gzip-compressed. Header: "JSL1", u32 nregions, then nregions x (u32 addr, u32 len). Per frame: u64 frame, the region bytes in
 * order, then 512 u32 hashes (FNV-1a over 4 KB pages of main RAM 0x80000000..0x80200000, 32-bit words), then 3 u64: cumulative vblank-IRQ guest cycles,
 * cumulative exception cycles, guest cycle counter. */
typedef struct { uint32_t addr, len; } Region;
static const Region k_regions[] = {
    { 0x800CD268u, 0x1208u },   /* the four fight objects block: companions + P1 + P2 (A4: 0x800CD268..0x800CE470) */
    { 0x8006CE68u, 0x70u },     /* raw pad buffers 0x8006CE68, 0x8006CE90 and per-pad records 0x8006CEB8, 0x8006CEC8 */
    { 0x80066C48u, 0x90u },     /* hit-stop flags/counter (0x80066CB0/1) and the fight globals */
    { 0x80066730u, 0x20u },     /* Capcom clock counters written by the vblank callback 0x80010BD4 (frames, seconds, minutes, count) */
    { 0x800739B0u, 0x4u },      /* callback tick counter (halfword at 0x800739B2) */
};
#define NREG ((int)(sizeof k_regions / sizeof k_regions[0]))
static gzFile g_log; static uint64_t g_log_from, g_log_to, g_log_frames; static char g_log_path[512];

static void log_close(void) {
    if (g_log) { gzclose(g_log); g_log = NULL; fprintf(stderr, "jojo-core: state log %s closed after %llu frames\n", g_log_path, (unsigned long long)g_log_frames); }
}
static int log_open(const char *path, uint64_t from, uint64_t to) {
    log_close();
    g_log = gzopen(path, "wb1");
    if (!g_log) return 0;
    strncpy(g_log_path, path, sizeof g_log_path - 1); g_log_path[sizeof g_log_path - 1] = 0;
    g_log_from = from; g_log_to = to; g_log_frames = 0;
    uint32_t n = NREG; gzwrite(g_log, "JSL1", 4); gzwrite(g_log, &n, 4);
    for (int i = 0; i < NREG; i++) { gzwrite(g_log, &k_regions[i].addr, 4); gzwrite(g_log, &k_regions[i].len, 4); }
    return 1;
}

static int g_env_checked;
static void maybe_autostart(void) {
    g_env_checked = 1;
    const char *p = getenv("JOJO_STATE_LOG");
    if (p && *p) {
        const char *f = getenv("JOJO_STATE_FROM"), *t = getenv("JOJO_STATE_TO");
        if (!log_open(p, f ? strtoull(f, NULL, 0) : 0, t ? strtoull(t, NULL, 0) : ~0ull)) fprintf(stderr, "jojo-core: cannot open state log %s\n", p);
        else { atexit(log_close); fprintf(stderr, "jojo-core: state log -> %s (frames %llu..%llu)\n", p, (unsigned long long)g_log_from, (unsigned long long)g_log_to); }
    }
}

/* JOJO_RAM_DUMP=<prefix>:<frame>,<frame>,...  writes the 2 MB of main RAM at those frames to <prefix>.f<frame>.bin (for diffing two runs). */
static void maybe_ram_dump(uint64_t frame) {
    static int parsed; static char prefix[400]; static uint64_t frames[64]; static int n;
    if (!parsed) {
        parsed = 1; const char *e = getenv("JOJO_RAM_DUMP");
        if (e && strchr(e, ':')) {
            size_t pl = (size_t)(strchr(e, ':') - e); if (pl >= sizeof prefix) pl = sizeof prefix - 1;
            memcpy(prefix, e, pl); prefix[pl] = 0;
            for (const char *c = e + pl + 1; *c && n < 64; ) { frames[n++] = strtoull(c, NULL, 0); while (*c && *c != ',') c++; if (*c == ',') c++; }
        }
    }
    for (int i = 0; i < n; i++) if (frames[i] == frame) {
        char path[512]; snprintf(path, sizeof path, "%s.f%llu.bin", prefix, (unsigned long long)frame);
        FILE *f = fopen(path, "wb"); if (!f) return;
        for (uint32_t a = 0x80000000u; a < 0x80200000u; a += 4) { uint32_t w = 0; psx_peek_word(a, &w); fwrite(&w, 4, 1, f); }
        fclose(f);
    }
}

/* Per-frame hooks of the pieces; weak so a build without a piece still links. */
extern void jojo_gpu_frame(void) __attribute__((weak));
extern void jojo_audio_frame(void) __attribute__((weak));

void jojo_frame_hook(uint64_t frame) {
    if (jojo_gpu_frame && jojo_piece_on(JOJO_PIECE_GPU)) jojo_gpu_frame();
    if (jojo_audio_frame && jojo_piece_on(JOJO_PIECE_AUDIO)) jojo_audio_frame();
    maybe_ram_dump(frame);
    if (!g_env_checked) maybe_autostart();
    if (!g_log || frame < g_log_from) return;
    if (frame > g_log_to) { log_close(); return; }
    gzwrite(g_log, &frame, 8);
    for (int r = 0; r < NREG; r++) {
        for (uint32_t o = 0; o < k_regions[r].len; o += 4) { uint32_t w = 0; psx_peek_word(k_regions[r].addr + o, &w); gzwrite(g_log, &w, 4); }
    }
    uint32_t h[512];
    for (int pg = 0; pg < 512; pg++) {
        uint32_t x = 2166136261u;
        for (uint32_t o = 0; o < 4096; o += 4) { uint32_t w = 0; psx_peek_word(0x80000000u + (uint32_t)pg * 4096u + o, &w); x = (x ^ w) * 16777619u; }
        h[pg] = x;
    }
    gzwrite(g_log, h, sizeof h);
    extern uint64_t g_exc_src_cycles[12], g_exc_cycles_total;
    uint64_t t[3] = { g_exc_src_cycles[0], g_exc_cycles_total, psx_cycle_count };
    gzwrite(g_log, t, sizeof t);
    g_log_frames++;
}

/* ---------------------------------------------------------------- "jojo" debug command */
int jojo_json_int(const char *json, const char *key, int def) {
    char pat[64]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = strstr(json, pat); if (!p) return def;
    p = strchr(p + strlen(pat), ':'); if (!p) return def;
    return (int)strtol(p + 1, NULL, 0);
}
int jojo_json_str(const char *json, const char *key, char *dst, size_t n) {
    char pat[64]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = strstr(json, pat); if (!p) return 0;
    p = strchr(p + strlen(pat), '"'); if (!p) return 0;
    const char *q = strchr(p + 1, '"'); if (!q || (size_t)(q - p - 1) >= n) return 0;
    memcpy(dst, p + 1, (size_t)(q - p - 1)); dst[q - p - 1] = 0; return 1;
}

static void put(char **o, char *end, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void put(char **o, char *end, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); int n = vsnprintf(*o, (size_t)(end - *o), fmt, ap); va_end(ap);
    if (n > 0) *o += (n < end - *o) ? n : (end - *o - 1);
}

int jojo_debug_command(const char *json, char *out, size_t out_size) {
    char op[32];
    if (!jojo_json_str(json, "op", op, sizeof op)) { snprintf(out, out_size, "missing op"); return 0; }
    char *o = out, *end = out + out_size;
    if (!strcmp(op, "state_log")) {
        char path[512];
        if (!jojo_json_str(json, "path", path, sizeof path)) { snprintf(out, out_size, "missing path"); return 0; }
        if (!log_open(path, (uint64_t)(unsigned)jojo_json_int(json, "from", 0), (uint64_t)(unsigned)jojo_json_int(json, "to", -1))) { snprintf(out, out_size, "cannot open path"); return 0; }
        g_env_checked = 1; put(&o, end, "{\"logging\":true}"); return 1;
    }
    if (!strcmp(op, "state_log_stop")) { uint64_t n = g_log_frames; log_close(); put(&o, end, "{\"frames\":%llu}", (unsigned long long)n); return 1; }
    if (!strcmp(op, "pieces")) {
        if (g_piece_state < 0) read_pieces();
        put(&o, end, "{");
        for (int i = 0; i < JOJO_PIECE_COUNT; i++) put(&o, end, "%s\"%s\":%d", i ? "," : "", k_piece_name[i], (g_piece_mask >> i) & 1);
        put(&o, end, "}"); return 1;
    }
    if (!strcmp(op, "counts")) {
        /* Optional "fn":"<hex>" limits the output to one function. Without it the output can exceed the 8 KB reply buffer of the
         * debug server, which truncates the JSON and drops the socket (seen with the scheduler harness runs, 2026-10-13). */
        char fnbuf[16]; uint32_t only = 0; int filtered = jojo_json_str(json, "fn", fnbuf, sizeof fnbuf);
        if (filtered) only = (uint32_t)strtoul(fnbuf, NULL, 16);
        int first = 1;
        put(&o, end, "{\"functions\":[");
        for (int i = 0; i < g_nfn; i++) {
            if (filtered && g_fn[i].addr != only) continue;
            put(&o, end, "%s{\"addr\":\"0x%08X\",\"total\":%llu,\"regions\":{", first ? "" : ",", g_fn[i].addr, (unsigned long long)g_fn[i].total);
            first = 0;
            for (int r = 0; r < g_fn[i].nreg; r++) {
                uint32_t k = g_fn[i].reg[r].key;
                if (k == 0) put(&o, end, "%s\"exe\":%llu", r ? "," : "", (unsigned long long)g_fn[i].reg[r].n);
                else if (k & 0x80000000u) put(&o, end, "%s\"ovl_0x80%02X0000\":%llu", r ? "," : "", (k & 0xFFFFu) , (unsigned long long)g_fn[i].reg[r].n);
                else put(&o, end, "%s\"other%u\":%llu", r ? "," : "", k, (unsigned long long)g_fn[i].reg[r].n);
            }
            put(&o, end, "},\"cycles\":{\"n\":%llu,\"avg\":%.1f,\"min\":%llu,\"max\":%llu}}", (unsigned long long)g_fn[i].cyc_n, g_fn[i].cyc_n ? (double)g_fn[i].cyc_sum / (double)g_fn[i].cyc_n : 0.0, (unsigned long long)(g_fn[i].cyc_n ? g_fn[i].cyc_min : 0), (unsigned long long)g_fn[i].cyc_max);
        }
        put(&o, end, "],\"devices\":{");
        for (int d = 0; d < JOJO_DEV_COUNT; d++) put(&o, end, "%s\"%s\":%llu", d ? "," : "", k_dev_name[d], (unsigned long long)g_dev[d]);
        put(&o, end, "}}"); return 1;
    }
    if (!strcmp(op, "counts_reset")) { g_nfn = 0; memset(g_dev, 0, sizeof g_dev); put(&o, end, "{}"); return 1; }
    snprintf(out, out_size, "unknown op %s", op); return 0;
}
