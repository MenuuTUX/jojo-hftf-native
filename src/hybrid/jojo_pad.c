/* H1 hybrid glue, piece "pad": wraps the guest pad-consumption routine and replaces the five library pad-driver entry points Capcom's pad setup calls.
 * Raw pad buffers are published immediately before guest consumption. DEV STAGE ONLY (it touches CPUState and guest addresses).
 *
 * What the original does (READ 0x80010F60, 0x80011020, 0x8004D8A0, 0x8004FD40; LIVE trace runs/phase1-015, state log runs/phase1-014):
 *   pad_init(buf0, buf1)  0x8004FD40  library driver setup; the library marks byte 0 of both raw buffers 0xFF and, two frames later,
 *                                     fills buf0 with {0x00, 0x41, lo, hi} (digital pad answered); buf1 stays 0xFF (nothing in port 2)
 *   pad_start()           0x8004D8A0  starts the SIO polling driver
 *   PadGetState(port)     0x8004D92C  called once per frame per answering pad by 0x80011020; Capcom needs: state 1 clears the "configured" flag
 *                                     and forces the buttons to 0, state 2 sets the flag after PadSetAct was called (11 calls in the reference)
 *   PadSetAct(port,p,2)   0x8004DCEC  called while the flag is clear (vibration data buffer); result ignored
 *   PadSetActAlign        0x8004DC6C  called only when the state is 6 (analog pad); not reached with a digital pad
 * The native path publishes at the guest's pre-consumption callback, supports digital pads only, and retains the measured PadGetState sequence.
 * The optional cycle table is a timing diagnostic, not a correctness proof. Library-internal memory (0x80063580..0x800636xx, the driver's tables) is not reproduced:
 * nothing but the replaced functions reads it.
 *
 * Switch: JOJO_PHASE1=all or JOJO_PAD=1 (default off: every wrapper calls the original generated body). */
#include "jojo_guest.h"
#include "jojo_core.h"
#include "pad.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void jojo_lle_func_80010FD8(CPUState *cpu);
extern void jojo_lle_func_8004FD40(CPUState *cpu);
extern void jojo_lle_func_8004D8A0(CPUState *cpu);
extern void jojo_lle_func_8004D92C(CPUState *cpu);
extern void jojo_lle_func_8004DCEC(CPUState *cpu);
extern void jojo_lle_func_8004DC6C(CPUState *cpu);

extern void psx_write_byte(uint32_t addr, uint8_t val);
extern uint8_t psx_read_byte(uint32_t addr);
extern uint16_t sio_get_pad_buttons_slot(int slot);
extern int  sio_get_pad_connected(int slot);
extern int  sio_get_pad_analog(int slot);
extern void sio_get_pad_sticks(int slot, uint8_t out[4]);

static uint32_t g_buf[2];             /* guest addresses of the two raw buffers, from pad_init */
static int      g_inited, g_started;
static uint64_t g_init_frame;
static unsigned g_state_calls[2];
static uint64_t g_diagnostic_frame;
static int g_have_diagnostic_frame;

#ifndef JOJO_PAD_DELAY
#define JOJO_PAD_LAG_DEFAULT 1
#define JOJO_PAD_DELAY 2              /* frames between pad_init and the first valid report (library: FindPad handshake), measured in the reference */
#endif
#ifndef JOJO_PAD_CYC_INIT
#define JOJO_PAD_CYC_INIT 87          /* guest cycles the original bodies used: mean of the "counts" cycles in runs/phase1-028 (REFERENCE, 3,000 frames) */
#define JOJO_PAD_CYC_START 14
#define JOJO_PAD_CYC_STATE 73
#define JOJO_PAD_CYC_ACT 26
#define JOJO_PAD_CYC_ALIGN 0
#define JOJO_PAD_CYC_FRAME 0          /* per-vblank cost of the library's polling hook, charged at the frame hook */
#endif
#define STATE_FINDING_CALLS 10        /* PadGetState calls that return 1 before it returns 2 (measured: PadSetAct is called 11 times) */

#define LLE_CALL(addr, fn) do { uint64_t c0_ = jojo_cycles_now(); fn(cpu); jojo_note_cycles((addr), jojo_cycles_now() - c0_); } while (0)
#define TRACE4(a) jojo_trace_call((a), a0, a1, a2, a3, cpu->gpr[2])
#define DONE(a) do { TRACE4(a); jojo_return(cpu); } while (0)

static void unsupported(const char *fn, uint32_t a0, uint32_t a1, uint32_t a2) {
    fprintf(stderr, "jojo-pad: unsupported %s args a0=%08X a1=%08X a2=%08X\n", fn, a0, a1, a2);
    abort();
}

static int port_arg(uint32_t a0, const char *fn, uint32_t a1, uint32_t a2) {
    if (a0 == 0) return 0;
    if (a0 == 0x10) return 1;
    unsupported(fn, a0, a1, a2);
    return 0;
}

/* The original driver polls the pad over SIO after the vblank, so the game sees the controller state of the previous vblank. The native path keeps
 * that one-frame latency with an explicit delay line, filled once per vblank by jojo_pad_frame() (called right after the host state reaches the SIO model).
 * JOJO_PAD_LAG (vblanks of latency) and JOJO_PAD_DELAY (vblanks between pad_init and the first valid report) default to the values measured against
 * the reference in runs/phase1-040..; the environment overrides are for calibration only. */
#define HIST 8
static PlatPad  g_hist[HIST][PLAT_PAD_PORTS];
static uint64_t g_nsamp;
static int g_lag = JOJO_PAD_LAG_DEFAULT, g_delay = JOJO_PAD_DELAY, g_cfg_read;

static void read_cfg(void) {
    g_cfg_read = 1;
    const char *e = getenv("JOJO_PAD_LAG");   if (e && *e) g_lag = atoi(e);
    e = getenv("JOJO_PAD_DELAY");             if (e && *e) g_delay = atoi(e);
    if (g_lag < 0 || g_lag >= HIST) g_lag = JOJO_PAD_LAG_DEFAULT;
}

static void sample_host_pads(void) {
    for (int port = 0; port < PLAT_PAD_PORTS; port++) {
        int type = sio_get_pad_analog(port);
        if (type != 0) unsupported("analog pad", (uint32_t)port, (uint32_t)type, 0);
        PlatPad p;
        p.connected = (uint8_t)sio_get_pad_connected(port);
        p.buttons = sio_get_pad_buttons_slot(port);
        p.analog = 0;
        uint8_t st[4]; sio_get_pad_sticks(port, st);
        p.lx = st[0]; p.ly = st[1]; p.rx = st[2]; p.ry = st[3];
        g_hist[g_nsamp % HIST][port] = p;
    }
    g_nsamp++;
}

/* Game-frame route (test harness): JOJO_ROUTE_GAME=<file> of lines "<game frame> <pad word hex> <frames>" drives port 0, indexed by
 * game frames (main-loop passes, counted from the byte 0x80066C61 the main loop increments) instead of vblanks. Two builds whose
 * loading lags differ by a frame then still feed the game the same input per game frame. tools-phase2/route_to_game.py converts a
 * vblank route with a run's state log. */
static struct { uint64_t start, end; uint16_t word; } *g_rt;
static size_t g_nrt;
static int g_rt_read;
static uint64_t g_gf;          /* game frames counted so far */
static uint8_t g_last_c61;
static int g_have_c61;

static void route_load(void) {
    const char *e = getenv("JOJO_ROUTE_GAME");
    FILE *f;
    unsigned long long st, n; unsigned w; size_t cap = 0;
    g_rt_read = 1;
    if (!e || !(f = fopen(e, "r"))) return;
    while (fscanf(f, "%llu %x %llu", &st, &w, &n) == 3) {
        if (g_nrt == cap) { cap = cap ? cap * 2 : 1024; g_rt = realloc(g_rt, cap * sizeof *g_rt); if (!g_rt) { fclose(f); g_nrt = 0; return; } }
        g_rt[g_nrt].start = st; g_rt[g_nrt].end = st + n; g_rt[g_nrt].word = (uint16_t)w; ++g_nrt;
    }
    fclose(f);
    fprintf(stderr, "jojo-pad: game-frame route %s, %zu steps\n", e, g_nrt);
}

static int route_word(uint16_t *out) {
    uint8_t c = psx_read_byte(0x80066C61u);
    size_t lo = 0, hi;
    if (!g_rt_read) route_load();
    if (!g_nrt) return 0;
    if (g_have_c61) g_gf += (uint8_t)(c - g_last_c61);
    g_last_c61 = c; g_have_c61 = 1;
    *out = 0xFFFF;
    for (hi = g_nrt; lo < hi; ) { size_t mid = (lo + hi) / 2; if (g_rt[mid].end <= g_gf) lo = mid + 1; else hi = mid; }
    if (lo < g_nrt && g_rt[lo].start <= g_gf && g_gf < g_rt[lo].end) *out = g_rt[lo].word;
    return 1;
}

static void publish_delayed(uint64_t frame) {
    uint16_t rw;
    int routed = route_word(&rw);
    if (frame < g_init_frame + (uint64_t)g_delay) return;
    for (int port = 0; port < PLAT_PAD_PORTS; port++) {
        uint64_t idx = g_nsamp > (uint64_t)g_lag + 1 ? g_nsamp - 1 - (uint64_t)g_lag : 0;
        PlatPad p = g_hist[idx % HIST][port];
        if (routed && port == 0) { p.connected = 1; p.buttons = rw; }
        plat_pad_set(port, &p);
        if (!p.connected) { psx_write_byte(g_buf[port], 0xFF); continue; }
        uint8_t rep[8]; size_t n = plat_pad_report(port, rep, sizeof rep);
        for (size_t i = 0; i < n; i++) psx_write_byte(g_buf[port] + (uint32_t)i, rep[i]);
    }
}

/* The 0x80010FD8 routine reads both raw buffers before PadGetState. Its entry is dispatched separately from its saved body. */
void func_80010FD8(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80010FD8, cpu->gpr[31]);
    if (getenv("JOJO_PAD_DEBUG")) { uint64_t fr = jojo_frame_now(); if (fr >= 2645 && fr <= 2662) fprintf(stderr, "pad consume frame=%llu\n", (unsigned long long)fr); }
    if (!jojo_piece_on(JOJO_PIECE_PAD)) {
        LLE_CALL(0x80010FD8, jojo_lle_func_80010FD8);
        TRACE4(0x80010FD8);
        return;
    }
    LLE_CALL(0x80010FD8, jojo_lle_func_80010FD8);
    TRACE4(0x80010FD8);
}

void func_8004FD40(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x8004FD40, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_PAD)) { LLE_CALL(0x8004FD40, jojo_lle_func_8004FD40); TRACE4(0x8004FD40); return; }
    g_buf[0] = a0; g_buf[1] = a1; g_inited = 1; g_init_frame = jojo_frame_now();
    g_have_diagnostic_frame = 0;
    g_state_calls[0] = g_state_calls[1] = 0;
    psx_write_byte(a0, 0xFF); psx_write_byte(a1, 0xFF);
    cpu->gpr[2] = 0;
    jojo_cycles_charge(JOJO_PAD_CYC_INIT);
    DONE(0x8004FD40);
}

void func_8004D8A0(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x8004D8A0, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_PAD)) { LLE_CALL(0x8004D8A0, jojo_lle_func_8004D8A0); TRACE4(0x8004D8A0); return; }
    g_started = 1;
    cpu->gpr[2] = 0;
    jojo_cycles_charge(JOJO_PAD_CYC_START);
    DONE(0x8004D8A0);
}

void func_8004D92C(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x8004D92C, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_PAD)) { LLE_CALL(0x8004D92C, jojo_lle_func_8004D92C); TRACE4(0x8004D92C); return; }
    int port = port_arg(a0, "PadGetState", a1, a2);
    unsigned n = ++g_state_calls[port];
    cpu->gpr[2] = (g_started && n > STATE_FINDING_CALLS) ? 2u : 1u;
    jojo_cycles_charge(JOJO_PAD_CYC_STATE);
    DONE(0x8004D92C);
}

void func_8004DCEC(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x8004DCEC, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_PAD)) { LLE_CALL(0x8004DCEC, jojo_lle_func_8004DCEC); TRACE4(0x8004DCEC); return; }
    (void)port_arg(a0, "PadSetAct", a1, a2);
    if (a2 != 2) unsupported("PadSetAct", a0, a1, a2);
    cpu->gpr[2] = 0;                  /* vibration buffer: no actuator yet (haptics come with the shell) */
    jojo_cycles_charge(JOJO_PAD_CYC_ACT);
    DONE(0x8004DCEC);
}

void func_8004DC6C(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x8004DC6C, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_PAD)) { LLE_CALL(0x8004DC6C, jojo_lle_func_8004DC6C); TRACE4(0x8004DC6C); return; }
    unsupported("PadSetActAlign (analog only)", a0, a1, a2);
}

/* Per-vblank hook (hybrid/psxrecomp_patches/pad_frame_hook.patch). Samples the host pads, publishes the delayed sample into the raw buffers and
 * charges the cycles the original polling hook spent (JOJO_CYCLE_TABLE is a timing diagnostic, not a correctness dependency). */
/* File routes (test harness, both builds): JOJO_ROUTE_P1=<file> and JOJO_ROUTE_P2=<file>, lines "<vblank frame> <pad word hex>
 * <frames>" in frame order. The words are applied after the runtime has sampled the pads for this vblank (pad_frame_hook.patch),
 * so they win, and before the pad piece reads the SIO model; 0xFFFF between steps. Port 2 gets a digital pad plugged in from boot
 * (two-player modes need it). Read at boot, so nothing races the game the way a TCP route install does on long routes. */
extern void sio_set_pad_state_slot(int slot, uint16_t buttons);
extern void sio_set_pad_connected(int slot, int connected);
extern void sio_set_pad_analog(int slot, int enabled, uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry);
typedef struct { uint64_t start, end; uint16_t word; } FileStep;
static struct { FileStep *s; size_t n, at; int state; } g_fr[2];   /* state: 0 not read, 1 no route, 2 active */

static void file_route_apply(int slot, const char *var) {
    uint64_t fr = jojo_frame_now();
    uint16_t w = 0xFFFFu;
    if (g_fr[slot].state == 0) {
        const char *e = getenv(var);
        FILE *f; unsigned long long st, n; unsigned x; size_t cap = 0;
        g_fr[slot].state = 1;
        if (!e || !*e || !(f = fopen(e, "r"))) return;
        while (fscanf(f, "%llu %x %llu", &st, &x, &n) == 3) {
            if (g_fr[slot].n == cap) { cap = cap ? cap * 2 : 256; g_fr[slot].s = realloc(g_fr[slot].s, cap * sizeof(FileStep)); if (!g_fr[slot].s) { fclose(f); g_fr[slot].n = 0; return; } }
            g_fr[slot].s[g_fr[slot].n].start = st; g_fr[slot].s[g_fr[slot].n].end = st + n; g_fr[slot].s[g_fr[slot].n].word = (uint16_t)x; ++g_fr[slot].n;
        }
        fclose(f);
        g_fr[slot].state = 2;
        if (slot == 1) { sio_set_pad_connected(1, 1); sio_set_pad_analog(1, 0, 0x80, 0x80, 0x80, 0x80); }
        fprintf(stderr, "jojo-pad: port-%d route %s, %zu steps\n", slot + 1, e, g_fr[slot].n);
    }
    if (g_fr[slot].state != 2) return;
    while (g_fr[slot].at < g_fr[slot].n && g_fr[slot].s[g_fr[slot].at].end <= fr) ++g_fr[slot].at;
    if (g_fr[slot].at < g_fr[slot].n && g_fr[slot].s[g_fr[slot].at].start <= fr) w = g_fr[slot].s[g_fr[slot].at].word;
    if (slot == 1) sio_set_pad_connected(1, 1);
    sio_set_pad_state_slot(slot, w);
}

static void route_p2_apply(void) {
    file_route_apply(0, "JOJO_ROUTE_P1");
    file_route_apply(1, "JOJO_ROUTE_P2");
}

/* RAM fixture (test harness, both builds): JOJO_RAM_OR="ADDR=MASK,...@FROM[;ADDR=MASK,...@FROM]" ORs each mask byte into guest RAM
 * on every vblank from its group's frame FROM on: unlock bits for gated characters (word 0x80066730, from before character select;
 * runs/phase3-continuation/selection_tables.md) and the per-fighter CPU flag (0x800CDF4D for P1, +0x390 for P2, from the fight
 * start, since that block holds other data in the menus). OR keeps every other bit, and both builds apply it at the same vblank. */
extern uint8_t *memory_get_ram_ptr(void);
static struct { uint32_t addr; uint8_t mask; uint64_t from; } g_or[32];
static int g_nor = -1;

static void ram_or_apply(void) {
    if (g_nor < 0) {
        const char *e = getenv("JOJO_RAM_OR");
        g_nor = 0;
        if (!e || !*e) return;
        for (const char *grp = e; *grp && g_nor < 32; ) {
            const char *semi = strchr(grp, ';'), *gend = semi ? semi : grp + strlen(grp), *at = memchr(grp, '@', (size_t)(gend - grp));
            uint64_t from = at ? strtoull(at + 1, NULL, 10) : 0;
            for (const char *p = grp; p < (at ? at : gend) && g_nor < 32; ) {
                char *q; unsigned long a = strtoul(p, &q, 16), m;
                if (*q != '=') break;
                m = strtoul(q + 1, &q, 16);
                g_or[g_nor].addr = (uint32_t)a; g_or[g_nor].mask = (uint8_t)m; g_or[g_nor].from = from; ++g_nor;
                p = (*q == ',') ? q + 1 : q;
            }
            grp = semi ? semi + 1 : gend;
        }
        fprintf(stderr, "jojo-pad: RAM fixture, %d byte(s)\n", g_nor);
    }
    for (int i = 0; i < g_nor; ++i)
        if (jojo_frame_now() >= g_or[i].from) memory_get_ram_ptr()[g_or[i].addr & 0x1FFFFFu] |= g_or[i].mask;
}

/* RAM set fixture (test harness, both builds): JOJO_RAM_SET="ADDR=BYTE@FROM-TO[;...]" writes the byte on every vblank in
 * [FROM, TO]. Used to load a character that has no select-screen path (encounter-only PL09/PL0C/PL0D/PL15) by setting the final
 * match ID (P1 0x8006D2D4, P2 0x8006D2D5) after the menu confirmed a selectable character and before the loaders read it. */
static struct { uint32_t addr; uint8_t val; uint64_t from, to; } g_set[16];
static int g_nset = -1;

static void ram_set_apply(void) {
    if (g_nset < 0) {
        const char *e = getenv("JOJO_RAM_SET");
        g_nset = 0;
        for (const char *p = e ? e : ""; *p && g_nset < 16; ) {
            char *q; unsigned long a = strtoul(p, &q, 16), v, f = 0, t = 0;
            if (*q != '=') break;
            v = strtoul(q + 1, &q, 16);
            if (*q == '@') { f = strtoul(q + 1, &q, 10); if (*q == '-') t = strtoul(q + 1, &q, 10); }
            g_set[g_nset].addr = (uint32_t)a; g_set[g_nset].val = (uint8_t)v; g_set[g_nset].from = f; g_set[g_nset].to = t ? t : f; ++g_nset;
            p = (*q == ';' || *q == ',') ? q + 1 : q;
        }
        if (g_nset) fprintf(stderr, "jojo-pad: RAM set fixture, %d byte(s)\n", g_nset);
    }
    for (int i = 0; i < g_nset; ++i) {
        uint64_t fr = jojo_frame_now();
        if (fr >= g_set[i].from && fr <= g_set[i].to) memory_get_ram_ptr()[g_set[i].addr & 0x1FFFFFu] = g_set[i].val;
    }
}

void jojo_pad_frame(void) {
    route_p2_apply();
    ram_or_apply();
    ram_set_apply();
    if (!jojo_piece_on(JOJO_PIECE_PAD)) return;
    if (!g_cfg_read) read_cfg();
    sample_host_pads();
    if (getenv("JOJO_PAD_DEBUG")) { uint64_t fr = jojo_frame_now(); if (fr >= 2645 && fr <= 2662) fprintf(stderr, "padhook frame=%llu nsamp=%llu buttons=%04X init=%d started=%d\n", (unsigned long long)fr, (unsigned long long)g_nsamp, g_hist[(g_nsamp - 1) % HIST][0].buttons, g_inited, g_started); }
    if (!g_inited || !g_started) return;
    uint64_t frame = jojo_frame_now();
    if (g_have_diagnostic_frame && g_diagnostic_frame == frame) return;
    publish_delayed(frame);
    jojo_cycles_charge(jojo_frame_cycles(frame, JOJO_PAD_CYC_FRAME));
    g_diagnostic_frame = frame;
    g_have_diagnostic_frame = 1;
}
