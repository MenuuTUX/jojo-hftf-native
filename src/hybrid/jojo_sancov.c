/* Coverage-guided capture (development only; CMake JOJO_SANCOV, build dir build-san). The game C (game/) is compiled with
 * -fsanitize-coverage=trace-pc-guard (new edges) and -finstrument-functions-after-inlining (a call stack of game C functions);
 * this file is not instrumented.
 *
 * jojo_capture_entry calls jojo_sancov_hook(X) at every hooked call of a listed function X and counts it; the next game C
 * function entered for X is tagged as that hooked call. A code edge running for the first time is attributed to the innermost
 * tagged call on the C stack (a direct C callee is covered by replaying its hooked caller). Pass 1 (JOJO_SANCOV_OUT=<file>)
 * writes "<X> <hooked call number of X>" per new edge; pass 2 (JOJO_CAPTURE_CALLS=@<file>) captures exactly those calls
 * (jojo_sancov_guided_take). Both passes play the same deterministic route with the same listed functions.
 *
 * Function addresses map to guest addresses by name (fn_X, fn_X_body, man_X) through JOJO_SANCOV_MAP (`nm -n` of this binary;
 * tools-phase2/sancov_capture.py writes it); the ASLR slide comes from jojo_sancov_anchor. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NOINST __attribute__((no_instrument_function))
NOINST void jojo_sancov_anchor(void) {}

static uint32_t g_ng;
static uint8_t *g_seen;
static int g_ready;          /* 0 not set up, 1 ready, -1 off */
static FILE *g_out;

/* open-addressing tables */
#define FN 16384
static uintptr_t g_fa[FN]; static uint32_t g_fx[FN];          /* runtime function address -> guest address */
#define XN 8192
static uint32_t g_xk[XN], g_xc[XN];                          /* guest address -> hooked calls so far */
NOINST static uint32_t *xcount(uint32_t x) {
    uint32_t s = (x * 2654435761u >> 19) % XN;
    while (g_xk[s] && g_xk[s] != x) s = (s + 1) % XN;
    g_xk[s] = x; return &g_xc[s];
}
NOINST static uint32_t fn_x(uintptr_t a) {
    uint32_t s = (uint32_t)((a >> 2) * 2654435761u) % FN;
    while (g_fa[s] && g_fa[s] != a) s = (s + 1) % FN;
    return g_fa[s] == a ? g_fx[s] : 0;
}

NOINST void __sanitizer_cov_trace_pc_guard_init(uint32_t *start, uint32_t *stop) {
    if (start == stop || *start) return;
    for (uint32_t *p = start; p < stop; ++p) *p = ++g_ng;
}

NOINST static uint32_t name_x(const char *n) {
    const char *p = n[0] == '_' ? n + 1 : n;
    size_t pre = strncmp(p, "fn_", 3) == 0 ? 3 : strncmp(p, "man_", 4) == 0 ? 4 : 0;
    char hex[9];
    if (!pre || strlen(p) < pre + 8) return 0;
    memcpy(hex, p + pre, 8); hex[8] = 0;
    for (int i = 0; i < 8; ++i) if (!strchr("0123456789ABCDEF", hex[i])) return 0;
    if (p[pre + 8] && p[pre + 8] != '_') return 0;
    return (uint32_t)strtoul(hex, NULL, 16);
}

NOINST static void setup(void) {
    const char *mp = getenv("JOJO_SANCOV_MAP"), *op = getenv("JOJO_SANCOV_OUT");
    char line[512], name[400], ty[8]; unsigned long long a;
    uintptr_t slide = 0; int have = 0; uint32_t n = 0;
    g_ready = -1;
    if (!mp) return;
    FILE *m = fopen(mp, "r");
    if (!m) { fprintf(stderr, "JOJO_SANCOV: cannot open map %s\n", mp); return; }
    while (fgets(line, sizeof line, m))   /* first pass: the anchor */
        if (sscanf(line, "%llx %7s %399s", &a, ty, name) == 3 && strcmp(name, "_jojo_sancov_anchor") == 0) {
            slide = (uintptr_t)&jojo_sancov_anchor - (uintptr_t)a; have = 1; break;
        }
    if (!have) { fclose(m); fprintf(stderr, "JOJO_SANCOV: anchor missing from the map\n"); return; }
    rewind(m);
    while (fgets(line, sizeof line, m)) {
        if (sscanf(line, "%llx %7s %399s", &a, ty, name) != 3) continue;
        uint32_t x = name_x(name);
        if (!x) continue;
        uintptr_t ra = (uintptr_t)a + slide;
        uint32_t s = (uint32_t)((ra >> 2) * 2654435761u) % FN;
        while (g_fa[s] && g_fa[s] != ra) s = (s + 1) % FN;
        g_fa[s] = ra; g_fx[s] = x; ++n;
    }
    fclose(m);
    g_seen = calloc(g_ng + 1, 1);
    if (op) g_out = fopen(op, "w");
    fprintf(stderr, "JOJO_SANCOV ready: %u edges, %u game C functions mapped\n", g_ng, n);
    g_ready = 1;
}

/* C call stack of game functions: guest address and, for a tagged frame, the hooked call number */
#define DEPTH 4096
static struct { uint32_t x, call; } g_st[DEPTH];
static int g_sp;
static uint32_t g_pending;   /* the hooked call whose game C function enters next */

NOINST void jojo_sancov_hook(uint32_t addr) {
    if (g_ready == 0) setup();
    ++*xcount(addr);
    g_pending = addr;
}

NOINST void __cyg_profile_func_enter(void *fn, void *site) {
    (void)site;
    if (g_ready <= 0) { if (g_ready == 0) setup(); if (g_ready < 0) return; }
    uint32_t x = fn_x((uintptr_t)fn), call = 0;
    if (x && x == g_pending) { call = *xcount(x); g_pending = 0; }
    if (g_sp < DEPTH) { g_st[g_sp].x = x; g_st[g_sp].call = call; }
    ++g_sp;
}

NOINST void __cyg_profile_func_exit(void *fn, void *site) {
    (void)fn; (void)site;
    if (g_ready > 0 && g_sp > 0) --g_sp;
}

NOINST void __sanitizer_cov_trace_pc_guard(uint32_t *guard) {
    uint32_t i = *guard;
    if (g_ready <= 0 || !i || i > g_ng || g_seen[i]) return;
    g_seen[i] = 1;
    if (!g_out) return;
    for (int k = (g_sp < DEPTH ? g_sp : DEPTH) - 1; k >= 0; --k)
        if (g_st[k].call) { fprintf(g_out, "%08X %u\n", g_st[k].x, g_st[k].call); return; }
}

/* Pass 2: 1 when the hooked call of addr counted just now (jojo_sancov_hook) is listed. */
static uint64_t *g_t; static uint32_t g_tn; static int g_tload;
NOINST static int cmp64(const void *a, const void *b) { uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return x < y ? -1 : x > y; }
NOINST int jojo_sancov_guided_take(uint32_t addr) {
    if (!g_tload) {
        const char *e = getenv("JOJO_CAPTURE_CALLS");
        uint32_t cap = 1024; unsigned x, n;
        g_tload = 1; g_t = malloc(cap * sizeof *g_t);
        FILE *t = e && *e == '@' ? fopen(e + 1, "r") : NULL;
        while (t && fscanf(t, "%x %u", &x, &n) == 2) {
            if (g_tn == cap) { cap *= 2; g_t = realloc(g_t, cap * sizeof *g_t); }
            g_t[g_tn++] = ((uint64_t)x << 32) | n;
        }
        if (t) fclose(t);
        qsort(g_t, g_tn, sizeof *g_t, cmp64);
        fprintf(stderr, "JOJO_SANCOV guided capture: %u target calls\n", g_tn);
    }
    uint64_t key = ((uint64_t)addr << 32) | *xcount(addr);
    uint32_t lo = 0, hi = g_tn;
    while (lo < hi) { uint32_t mid = (lo + hi) / 2; if (g_t[mid] < key) lo = mid + 1; else hi = mid; }
    return lo < g_tn && g_t[lo] == key;
}
