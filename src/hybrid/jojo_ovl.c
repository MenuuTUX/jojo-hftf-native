/* Overlay function hooks (development only). See jojo_ovl.h. */
#include "jojo_ovl.h"
#include "jojo_replay.h"
#include "jojo_core.h"
#include "cpu_state.h"
#include "interrupts.h"
#include "mod_plugins.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint8_t *memory_get_ram_ptr(void);

/* Hooked addresses, open-addressed by address (OVL_MAX a power of two). Each address keeps one alternative per expected code CRC:
 * all characters load at the same two slot addresses, so one address can carry a native for each character's code. */
#define OVL_MAX 16384u
#define OVL_ALT 32u
typedef struct { uint32_t size, crc; JojoOvlFilter f; int (*matches)(void); } OvlAlt;
static struct { uint32_t addr; uint64_t entries, guard_reject; int active; uint32_t nalt; OvlAlt alt[OVL_ALT]; } g_o[OVL_MAX];
static uint32_t g_order[OVL_MAX], g_no;   /* slots in first-registration order, for the exit summary */
static uint32_t g_skip_addr;     /* one-shot: the next hook entry at this address is the reference run's own */
static uint32_t g_crc_ok;        /* the dispatcher already matched the code CRC for this address (jojo_ovl_enter skips its check) */
static int g_inited;

static uint32_t crc32_bytes(const uint8_t *p, uint32_t n) {
    uint32_t c = 0xFFFFFFFFu, i, k;
    for (i = 0; i < n; ++i) { c ^= p[i]; for (k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); }
    return ~c;
}

int jojo_ovl_matches(uint32_t addr, uint32_t size, uint32_t crc) {
    uint32_t off = addr & 0x1FFFFFu;
    return size <= 0x200000u - off && crc32_bytes(memory_get_ram_ptr() + off, size) == crc;
}

static void ovl_summary(void) {
    uint32_t i;
    for (i = 0; i < g_no; ++i)
        fprintf(stderr, "JOJO_OVL fn=0x%08X hook_entries=%llu guard_reject=%llu\n", g_o[g_order[i]].addr,
                (unsigned long long)g_o[g_order[i]].entries, (unsigned long long)g_o[g_order[i]].guard_reject);
}

static uint32_t slot(uint32_t addr) {
    uint32_t i = ((addr >> 2) * 2654435761u) & (OVL_MAX - 1u), n;
    for (n = 0; n < OVL_MAX; ++n, i = (i + 1u) & (OVL_MAX - 1u)) {
        if (g_o[i].addr == addr) return i;
        if (g_o[i].addr == 0) {
            if (g_no >= OVL_MAX - 1u) break;
            g_o[i].addr = addr;
            g_order[g_no++] = i;
            return i;
        }
    }
    fprintf(stderr, "JOJO_OVL: hook table full at 0x%08X\n", addr);
    abort();
}

/* The one runtime filter per address: picks the alternative whose expected code is at the address now. With one alternative the
 * generated filter does the check itself (behaviour of the single-hook version). */
static int ovl_dispatch(CPUState *cpu, uint32_t addr) {
    uint32_t s = slot(addr), i, sz = 0, c = 0;
    int r;
    if (g_skip_addr == addr) { g_skip_addr = 0; return 0; }
    if (getenv("JOJO_OVL_PASS")) { ++g_o[s].entries; return 0; }
    for (i = 0; i < g_o[s].nalt; ++i) {
        if (g_o[s].alt[i].size != sz) { sz = g_o[s].alt[i].size; c = crc32_bytes(memory_get_ram_ptr() + (addr & 0x1FFFFFu), sz); }
        if (c != g_o[s].alt[i].crc || (g_o[s].alt[i].matches && !g_o[s].alt[i].matches())) continue;
        g_crc_ok = addr;
        r = g_o[s].alt[i].f(cpu, addr);
        g_crc_ok = 0;
        return r;
    }
    ++g_o[s].entries;
    ++g_o[s].guard_reject;
    return 0;
}

void jojo_ovl_register(uint32_t addr, uint32_t size, uint32_t crc, JojoOvlFilter filter, int active) {
    jojo_ovl_register_variant(addr, size, crc, filter, active, NULL);
}

void jojo_ovl_register_variant(uint32_t addr, uint32_t size, uint32_t crc, JojoOvlFilter filter, int active, int (*matches)(void)) {
    uint32_t s, i;
    if (!active) return;
    if (!g_inited) {
        g_inited = 1;
        setenv("PSX_GAME_FILTER_OFFLINE_PROBE", "1", 1);   /* game filters run offline only with this (mod_runtime.cpp) */
        atexit(ovl_summary);
    }
    s = slot(addr);
    g_o[s].active = 1;
    for (i = 0; i < g_o[s].nalt; ++i)
        if (g_o[s].alt[i].crc == crc && g_o[s].alt[i].size == size && g_o[s].alt[i].matches == matches) { g_o[s].alt[i].f = filter; return; }   /* same code and module: last wins */
    if (g_o[s].nalt >= OVL_ALT) { fprintf(stderr, "JOJO_OVL: too many code variants at 0x%08X\n", addr); abort(); }
    g_o[s].alt[g_o[s].nalt].size = size;
    g_o[s].alt[g_o[s].nalt].crc = crc;
    g_o[s].alt[g_o[s].nalt].f = filter;
    g_o[s].alt[g_o[s].nalt].matches = matches;
    if (g_o[s].nalt++ == 0u && !psx_game_register_netplay_function_filter(addr, (PSXModFunctionFilterCallback)ovl_dispatch))
        fprintf(stderr, "JOJO_OVL: registering a hook for 0x%08X failed\n", addr);
}

int jojo_ovl_enter(CPUState *cpu, uint32_t addr, uint32_t size, uint32_t crc) {
    uint32_t s = slot(addr);
    ++g_o[s].entries;
    if (g_skip_addr == addr) { g_skip_addr = 0; --g_o[s].entries; return 0; }
    if (getenv("JOJO_OVL_PASS")) return 0;   /* coverage check: count the entry, run the original (see tools-phase2/ovl_coverage.py) */
    if (g_crc_ok != addr && crc32_bytes(memory_get_ram_ptr() + (addr & 0x1FFFFFu), size) != crc) { ++g_o[s].guard_reject; return 0; }
    if (jojo_capture_on()) {
        jojo_capture_entry(cpu, addr);
        if (!jojo_capture_live()) return 0;   /* default capture still runs the original */
    }
    return 1;
}

void jojo_ovl_ref(CPUState *cpu, uint32_t addr) {
    uint32_t ra = cpu->gpr[31];
    g_skip_addr = addr;
    psx_dispatch_call(cpu, addr, ra);
    g_skip_addr = 0;
    if (cpu->pc == 0) cpu->pc = ra;
    if (cpu->pc != ra) { fprintf(stderr, "JOJO_OVL: reference of 0x%08X stopped at pc=0x%08X, expected 0x%08X\n", addr, cpu->pc, ra); abort(); }
}

/* The planted change of a control run. negspad flips scratchpad byte JOJO_DIFF_NEG_SPAD (hex offset, default 0x5C, a game global):
 * proves scratchpad globals stay compared next to the excluded scratchpad stack window (jojo_diff.c). */
extern uint8_t *memory_get_scratchpad_ptr(void);
static void jojo_control_plant(CPUState *cpu, const char *mode) {
    if (strcmp(mode, "neg") == 0) memory_get_ram_ptr()[0x100100u] ^= 1u;
    else if (strcmp(mode, "negreg") == 0) cpu->gpr[2] += 1u;
    else if (strcmp(mode, "negspad") == 0) {
        const char *e = getenv("JOJO_DIFF_NEG_SPAD");
        memory_get_scratchpad_ptr()[(e ? strtoul(e, NULL, 16) : 0x5Cu) & 0x3FFu] ^= 1u;
    }
}

/* Harness controls for any hooked overlay function (JOJO_DIFF_CONTROL): aa = the native side is the reference itself, neg = the
 * reference plus one planted RAM change, negreg = the reference with v0 + 1. native / unset: not a control, return 0. */
int jojo_ovl_control(CPUState *cpu, uint32_t addr) {
    const char *mode = getenv("JOJO_DIFF_CONTROL");
    if (!mode || !*mode || strcmp(mode, "native") == 0) return 0;
    jojo_ovl_ref(cpu, addr);
    jojo_control_plant(cpu, mode);
    return 1;
}

/* The same controls for exe functions: `ref` is the glue's reference body (run to return). */
int jojo_exe_control(CPUState *cpu, void (*ref)(CPUState *)) {
    const char *mode = getenv("JOJO_DIFF_CONTROL");
    if (!mode || !*mode || strcmp(mode, "native") == 0) return 0;
    ref(cpu);
    jojo_control_plant(cpu, mode);
    return 1;
}
