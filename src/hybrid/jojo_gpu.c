/* H1 GPU adapter. Guest addresses and CPUState stay in hybrid glue; platform/gpu_* is C99 host-only code. */
#include "jojo_guest.h"
#include "jojo_core.h"
#include "gpu_raster.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void jojo_lle_func_80035D20(CPUState *cpu);
extern void jojo_lle_func_80036004(CPUState *cpu);
extern void jojo_lle_func_8003609C(CPUState *cpu);
extern void jojo_lle_func_800362B0(CPUState *cpu);
extern void jojo_lle_func_80036348(CPUState *cpu);
extern void jojo_lle_func_800364C0(CPUState *cpu);
extern void jojo_lle_func_80036588(CPUState *cpu);
extern void jojo_lle_func_80036634(CPUState *cpu);
extern void jojo_lle_func_80036690(CPUState *cpu);
extern void jojo_lle_func_80036700(CPUState *cpu);
extern void jojo_lle_func_800368CC(CPUState *cpu);
extern void jojo_lle_func_800389C0(CPUState *cpu);
extern void jojo_lle_func_800396F0(CPUState *cpu);
extern void jojo_lle_func_800397B0(CPUState *cpu);
extern void jojo_lle_func_800397F0(CPUState *cpu);
extern void jojo_lle_func_80039830(CPUState *cpu);
extern void jojo_lle_func_80039850(CPUState *cpu);
extern void jojo_lle_func_80039890(CPUState *cpu);
extern void jojo_lle_func_800398C0(CPUState *cpu);
extern void jojo_lle_func_800398E0(CPUState *cpu);
extern void jojo_lle_func_80039900(CPUState *cpu);
extern void jojo_lle_func_80039920(CPUState *cpu);
extern void jojo_lle_func_80039940(CPUState *cpu);
extern void jojo_lle_func_80039960(CPUState *cpu);
extern void jojo_lle_func_80039980(CPUState *cpu);
extern void jojo_lle_func_800399B0(CPUState *cpu);
extern void jojo_lle_func_800399F0(CPUState *cpu);
extern void jojo_lle_func_8003C4A0(CPUState *cpu);
extern void jojo_lle_func_8003C594(CPUState *cpu);

/* Runtime state at the point of a GP0/GP1 store (generated code sets g_debug_last_store_pc before every store; cpu->gpr is current). */
extern uint32_t g_debug_last_store_pc;
extern uint32_t debug_guest_ra(void);

extern uint8_t psx_read_byte(uint32_t addr);
extern uint16_t psx_read_half(uint32_t addr);
extern uint32_t psx_read_word(uint32_t addr);

#define GPU_ITEM_CAP GPU_RASTER_MAX_ITEMS
#define GPU_PAYLOAD_CAP (GPU_RASTER_MAX_ITEMS * 32u)
#define GPU_OT_NODE_CAP 65536u

static GpState g_gp_state;
static GpItem g_items[GPU_ITEM_CAP];
static uint32_t g_payload[GPU_PAYLOAD_CAP];
static uint64_t g_seen_unknown;
static int g_model_ready;
static GpuDisplayEnv g_display = { 0, 0, 384, 240, 1, 0, 0, 0 };

#define LLE_CALL(addr, fn) do { uint64_t c0_ = jojo_cycles_now(); fn(cpu); jojo_note_cycles((addr), jojo_cycles_now() - c0_); } while (0)
#define TRACE4(addr) jojo_trace_call((addr), a0, a1, a2, a3, cpu->gpr[2])
#define DONE(addr) do { TRACE4(addr); jojo_return(cpu); } while (0)

static void unsupported(const char *what, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    fprintf(stderr, "jojo-gpu: unsupported %s a0=%08X a1=%08X a2=%08X a3=%08X\n", what, a0, a1, a2, a3);
    abort();
}

static void ensure_model(void) {
    if (!g_model_ready) {
        const char *soft = getenv("JOJO_GPU_SOFT");   /* debugging switch: JOJO_GPU_SOFT=0 records draws without rasterising them */
        gp0_state_init(&g_gp_state);
        gpu_raster_set_display(&g_display);
        gpu_raster_set_soft(!(soft && !strcmp(soft, "0")));
        gpu_raster_set_prov(getenv("JOJO_GPU_PROV_DUMP") != NULL);
        g_model_ready = 1;
    }
}

static uint32_t read_guest_word(void *unused, uint32_t addr) {
    (void)unused;
    return psx_read_word(addr);
}

/* The decoded batch goes to the ordered stream in one call: VRAM fills, copies and uploads are applied to the CPU model and
 * recorded in guest order together with the draws, so a replay sees each draw against the VRAM it was issued into.
 * The frame is published once per host frame (jojo_gpu_frame); a frame that does not fit the stream is a hard error. */
static void consume_items(GpOut *out, uint32_t a0) {
    size_t i;
    for (i = 0; i < out->items_n; ++i) {
        GpItem *it = &out->items[i];
        switch (it->type) {
        case GP_ITEM_ENV:
        case GP_ITEM_FILL:
        case GP_ITEM_COPY_VRAM:
        case GP_ITEM_TRI:
        case GP_ITEM_QUAD:
        case GP_ITEM_LINE:
        case GP_ITEM_RECT:
            break;
        case GP_ITEM_COPY_TO_VRAM:
            if ((it->flags & GP_F_TRUNCATED) || it->payload_n < ((size_t)it->w * it->h + 1u) / 2u)
                unsupported("truncated CPU-to-VRAM transfer", a0, it->w, it->h, it->payload_n);
            break;
        case GP_ITEM_COPY_TO_CPU:
            unsupported("GP0 C0 readback command without GPUREAD consumer", a0, it->x[0], it->y[0], ((uint32_t)it->w << 16) | it->h);
            break;
        default:
            unsupported("decoded GP0 item", a0, it->type, it->flags, 0);
        }
    }
    if (gpu_raster_record(out->items, out->items_n, out->payload, out->payload_n) != 0)
        unsupported("frame stream capacity (GPU_RASTER_MAX_ITEMS or payload words)", a0, (uint32_t)out->items_n, GPU_ITEM_CAP, (uint32_t)out->payload_n);
}

static void decode_words(const uint32_t *words, size_t n, uint32_t caller) {
    GpOut out;
    int rc;
    ensure_model();
    gp0_out_init(&out, g_items, GPU_ITEM_CAP, g_payload, GPU_PAYLOAD_CAP);
    rc = gp0_decode_words(words, n, &g_gp_state, &out);
    if (rc != GP_OK || out.unknown || out.truncated) {
        g_seen_unknown += out.unknown;
        fprintf(stderr, "jojo-gpu: GP0 decode failed caller=%08X rc=%d unknown=%lu truncated=%lu\n",
                caller, rc, (unsigned long)out.unknown, (unsigned long)out.truncated);
        abort();
    }
    consume_items(&out, caller);
}

static void replay_check_reset(void);

/* JOJO_GPU_SHADOW=1 (diagnostics, piece GPU on): every native GPU operation is also sent as GP0 words to the runtime's emulated
 * GPU, and at each frame boundary the native VRAM model is compared with the runtime's VRAM. Both receive the same commands in
 * the same order in the same run, so a difference is a decoder or rasteriser difference and never a timing one.
 * JOJO_GPU_SHADOW_DUMP=<prefix> also writes <prefix>.f<frame>.{nat,ref}.vram for the first 12 differing frames. */
extern void gpu_write_gp0(uint32_t val);
extern const uint16_t *gpu_get_vram(void);
static int g_shadow_on = -1;
static int g_shadow_feeding;
static unsigned long g_shadow_frames, g_shadow_bad, g_shadow_dumps;
static unsigned long long g_shadow_px;
static uint16_t g_shadow_nat[GPU_RASTER_VRAM_PIXELS];

static void shadow_summary(void) {
    fprintf(stderr, "jojo-gpu: shadow: %lu frames compared, %lu with differences, %llu differing pixels in total\n",
            g_shadow_frames, g_shadow_bad, g_shadow_px);
}

static int shadow_on(void) {
    if (g_shadow_on < 0) {
        const char *e = getenv("JOJO_GPU_SHADOW");
        g_shadow_on = (e && *e && strcmp(e, "0") != 0) ? 1 : 0;
        if (g_shadow_on) atexit(shadow_summary);
    }
    return g_shadow_on;
}

static void shadow_feed(const uint32_t *w, size_t n) {
    size_t i;
    if (!shadow_on()) return;
    g_shadow_feeding = 1;
    for (i = 0; i < n; i++) gpu_write_gp0(w[i]);
    g_shadow_feeding = 0;
}

static void shadow_feed_ot(uint32_t first) {
    uint32_t addr = first;
    unsigned k, i;
    if (!shadow_on() || (first & 0xFFFFFFu) == 0xFFFFFFu) return;
    for (k = 0; k < GPU_OT_NODE_CAP; k++) {
        uint32_t hdr = psx_read_word(addr), next = hdr & 0xFFFFFFu, w[256];
        unsigned count = hdr >> 24;
        for (i = 0; i < count; i++) w[i] = psx_read_word(addr + 4u + 4u * i);
        shadow_feed(w, count);
        if (next == 0xFFFFFFu) return;
        addr = next & 0xFFFFFCu;
    }
}

static void shadow_compare(uint64_t frame) {
    const uint16_t *ref;
    size_t i, diff = 0;
    int x0 = 1024, y0 = 512, x1 = -1, y1 = -1;
    if (!shadow_on()) return;
    ref = gpu_get_vram();
    if (!ref || !gpu_raster_copy_live_vram(g_shadow_nat, GPU_RASTER_VRAM_PIXELS)) return;
    for (i = 0; i < GPU_RASTER_VRAM_PIXELS; i++) {
        if (g_shadow_nat[i] == ref[i]) continue;
        diff++;
        if ((int)(i % 1024u) < x0) x0 = (int)(i % 1024u);
        if ((int)(i % 1024u) > x1) x1 = (int)(i % 1024u);
        if ((int)(i / 1024u) < y0) y0 = (int)(i / 1024u);
        if ((int)(i / 1024u) > y1) y1 = (int)(i / 1024u);
    }
    g_shadow_frames++;
    if (!diff) return;
    g_shadow_bad++;
    g_shadow_px += diff;
    fprintf(stderr, "jojo-gpu: shadow frame %llu: %lu differing pixels, box %d,%d..%d,%d\n",
            (unsigned long long)frame, (unsigned long)diff, x0, y0, x1, y1);
    if (g_shadow_dumps < 12) {
        const char *p = getenv("JOJO_GPU_SHADOW_DUMP");
        if (p && *p) {
            char path[512];
            FILE *f;
            snprintf(path, sizeof path, "%s.f%llu.nat.vram", p, (unsigned long long)frame);
            if ((f = fopen(path, "wb"))) { fwrite(g_shadow_nat, 2, GPU_RASTER_VRAM_PIXELS, f); fclose(f); }
            snprintf(path, sizeof path, "%s.f%llu.ref.vram", p, (unsigned long long)frame);
            if ((f = fopen(path, "wb"))) { fwrite(ref, 2, GPU_RASTER_VRAM_PIXELS, f); fclose(f); }
            g_shadow_dumps++;
        }
    }
}

/* ---- GP0/GP1 attribution (JOJO_GPU_ATTR=<file>) and the streaming GP0 collector (piece GPU on) ----
 * Every GP0 and GP1 word of the runtime passes jojo_gp0_word / jojo_gp1_word (gpu.c hooks, see hybrid/psxrecomp_patches/
 * gpu_gp0_hook.patch). Attribution counts words by (kind, phase, store PC, caller $ra, command opcode). The collector frames the
 * same commands as the runtime's GP0 state machine and hands each complete command to the native decoder, so the GP0 traffic
 * that guest code issues directly (not through a wrapped entry point) reaches the native model in order. */
#define ATTR_SLOTS 65536u
typedef struct { uint32_t pc, ra; uint8_t kind, phase, op; uint64_t n; uint8_t used; } AttrSlot;
static AttrSlot g_attr[ATTR_SLOTS];
static uint64_t g_attr_words_total;
static int g_attr_on = -1;
static char g_attr_path[400];
static int g_attr_registered;

static void attr_write(void) {
    FILE *f = fopen(g_attr_path, "w");
    uint32_t i;
    if (!f) return;
    fprintf(f, "# kind(0=GP0,1=GP1) phase(0 boot,1 frames 345-2000,2 2001-4468,3 fight 4469+) store_pc caller_ra op(0xFF=parameter word) words\n");
    for (i = 0; i < ATTR_SLOTS; i++) {
        const AttrSlot *a = &g_attr[i];
        if (!a->used) continue;
        fprintf(f, "%u %u 0x%08X 0x%08X 0x%02X %llu\n", a->kind, a->phase, a->pc, a->ra, a->op, (unsigned long long)a->n);
    }
    fclose(f);
}

static void attr_note(uint8_t kind, uint8_t op, uint32_t pc, uint32_t ra) {
    uint64_t frame = jojo_frame_now();
    uint8_t phase = frame < 345 ? 0 : frame < 2001 ? 1 : frame < 4469 ? 2 : 3;
    uint32_t h = (pc * 2654435761u) ^ (ra * 40503u) ^ ((uint32_t)kind << 28) ^ ((uint32_t)phase << 24) ^ ((uint32_t)op << 16);
    uint32_t idx = h & (ATTR_SLOTS - 1u), probe;
    for (probe = 0; probe < ATTR_SLOTS; probe++) {
        AttrSlot *a = &g_attr[(idx + probe) & (ATTR_SLOTS - 1u)];
        if (!a->used) {
            a->used = 1; a->kind = kind; a->phase = phase; a->op = op; a->pc = pc; a->ra = ra; a->n = 0;
        }
        if (a->kind == kind && a->phase == phase && a->op == op && a->pc == pc && a->ra == ra) {
            a->n++;
            g_attr_words_total++;
            return;
        }
    }
}

static void attr_init(void) {
    const char *e;
    if (g_attr_on >= 0) return;
    e = getenv("JOJO_GPU_ATTR");
    g_attr_on = (e && *e) ? 1 : 0;
    if (g_attr_on) {
        snprintf(g_attr_path, sizeof g_attr_path, "%s", e);
        if (!g_attr_registered) { g_attr_registered = 1; atexit(attr_write); }
    }
}

/* Streaming collector. State: 0 idle (next word starts a command), 1 fixed-length command or transfer collecting,
 * 2 mono polyline, 3 shaded polyline. Polyline terminator rules as gpu.c gpu_write_gp0_body. */
static uint32_t g_coll_buf[GPU_PAYLOAD_CAP + 64u];
static size_t g_coll_n, g_coll_total;
static int g_coll_state;
static int g_coll_pl_prev;
static GpItem g_coll_items[GPU_ITEM_CAP];
static uint32_t g_coll_payload[GPU_PAYLOAD_CAP];
static uint64_t g_coll_commands;

static void coll_reset(void) {
    g_coll_n = 0;
    g_coll_total = 0;
    g_coll_state = 0;
    g_coll_pl_prev = 0;
}

static void coll_complete(void) {
    GpOut out;
    int rc;
    ensure_model();
    gp0_out_init(&out, g_coll_items, GPU_ITEM_CAP, g_coll_payload, GPU_PAYLOAD_CAP);
    rc = gp0_decode_words(g_coll_buf, g_coll_n, &g_gp_state, &out);
    if (rc != GP_OK || out.truncated) {
        fprintf(stderr, "jojo-gpu: collector decode rc=%d truncated=%lu words=%lu op=%02X\n", rc, (unsigned long)out.truncated,
                (unsigned long)g_coll_n, (unsigned)(g_coll_buf[0] >> 24));
        abort();
    }
    if (gpu_raster_record(out.items, out.items_n, out.payload, out.payload_n) != 0)
        unsupported("collector: frame stream capacity", g_coll_buf[0], (uint32_t)out.items_n, GPU_ITEM_CAP, 0);
    g_coll_commands++;
    coll_reset();
}

static void coll_word(uint32_t val) {
    if (g_coll_state == 0) {
        uint8_t op = (uint8_t)(val >> 24);
        int need;
        g_coll_buf[0] = val;
        g_coll_n = 1;
        if (op >= 0x40 && op <= 0x5F && (op & 0x08)) {          /* polyline: header, then vertex and colour words */
            g_coll_state = (op & 0x10) ? 3 : 2;
            g_coll_pl_prev = 0;
            return;
        }
        need = gp0_fixed_words(op);
        if (need <= 0) need = 1;                                   /* nops and undefined opcodes: one word */
        g_coll_total = (size_t)need;
        if (g_coll_total == 1) { coll_complete(); return; }
        g_coll_state = 1;
        return;
    }
    if (g_coll_state == 1) {
        if (g_coll_n >= sizeof g_coll_buf / sizeof g_coll_buf[0]) abort();
        g_coll_buf[g_coll_n++] = val;
        if (g_coll_n == 3 && (g_coll_buf[0] >> 24) >= 0xA0 && (g_coll_buf[0] >> 24) <= 0xBF) {
            unsigned w = g_coll_buf[2] & 0x3FFu, h = (g_coll_buf[2] >> 16) & 0x1FFu;
            if (!w) w = 1024u;
            if (!h) h = 512u;
            g_coll_total = 3u + ((size_t)w * h + 1u) / 2u;
        }
        if (g_coll_n == g_coll_total) coll_complete();
        return;
    }
    /* polylines */
    g_coll_buf[g_coll_n++] = val;
    if (g_coll_state == 2) {                                       /* mono: terminator tested from the third vertex on */
        if (g_coll_pl_prev >= 2 && (val & 0xF000F000u) == 0x50005000u) { coll_complete(); return; }
        if (g_coll_pl_prev < 2) g_coll_pl_prev++;
        return;
    }
    /* shaded: V0, C1, V1, then (C2, V2) pairs; only colour words of the third and later vertices are tested */
    if (g_coll_pl_prev == 0) { g_coll_pl_prev = 1; return; }
    if (g_coll_pl_prev == 1) { g_coll_pl_prev = 2; return; }
    if (g_coll_pl_prev == 2) { g_coll_pl_prev = 3; return; }
    if ((val & 0xF000F000u) == 0x50005000u) { coll_complete(); return; }
    g_coll_pl_prev = 2;
}

void jojo_gp0_word(uint32_t val) {
    if (g_shadow_feeding) return;
    attr_init();
    if (g_attr_on) attr_note(0, g_coll_state == 0 ? (uint8_t)(val >> 24) : 0xFFu, g_debug_last_store_pc, debug_guest_ra());
    if (jojo_piece_on(JOJO_PIECE_GPU)) coll_word(val);
}

void jojo_gp1_word(uint32_t val) {
    attr_init();
    if (g_attr_on) attr_note(1, (uint8_t)(val >> 24), g_debug_last_store_pc, debug_guest_ra());
}

static void reset_gpu_model(void) {
    gp0_state_init(&g_gp_state);
    coll_reset();
    gpu_raster_reset();
    replay_check_reset();
    g_display = (GpuDisplayEnv){ 0, 0, 384, 240, 1, 0, 0, 0 };
    gpu_raster_set_display(&g_display);
    g_model_ready = 1;
    g_seen_unknown = 0;
}

/* JOJO_GPU_VRAM_DUMP=<prefix>:<frame>,<frame>,... writes the live CPU VRAM model (1024 x 512 u16, row-major, little-endian)
 * at the frame boundary of each listed frame to <prefix>.f<frame>.vram. Diagnostics only (tools-phase1/gpu_compare). */
static uint16_t g_vram_dump[GPU_RASTER_VRAM_PIXELS];

static void maybe_vram_dump(uint64_t frame) {
    static int parsed;
    static char prefix[400];
    static uint64_t frames[64];
    static int n;
    unsigned i;
    if (!parsed) {
        const char *e;
        parsed = 1;
        e = getenv("JOJO_GPU_VRAM_DUMP");
        if (e && strchr(e, ':')) {
            const char *c;
            size_t pl = (size_t)(strchr(e, ':') - e);
            if (pl >= sizeof prefix) pl = sizeof prefix - 1;
            memcpy(prefix, e, pl);
            prefix[pl] = 0;
            for (c = e + pl + 1; *c && n < 64;) {
                frames[n++] = strtoull(c, NULL, 0);
                while (*c && *c != ',') c++;
                if (*c == ',') c++;
            }
        }
    }
    for (i = 0; i < (unsigned)n; i++) {
        char path[512];
        FILE *f;
        if (frames[i] != frame) continue;
        snprintf(path, sizeof path, "%s.f%llu.vram", prefix, (unsigned long long)frame);
        if (!gpu_raster_copy_live_vram(g_vram_dump, GPU_RASTER_VRAM_PIXELS)) return;
        f = fopen(path, "wb");
        if (!f) return;
        fwrite(g_vram_dump, sizeof g_vram_dump[0], GPU_RASTER_VRAM_PIXELS, f);
        fclose(f);
    }
}

/* JOJO_GPU_REPLAY_CHECK=1: at each frame boundary the stream published for the frame that just ended is replayed, in order,
 * onto the VRAM that the frame before it ended with, and the result is compared with that frame's end-of-frame VRAM. A
 * difference means the recorded stream does not reproduce the live model. Diagnostics only; a summary is printed at exit. */
static int g_check_on = -1;
static GpuVram g_check_model;
static uint16_t g_check_start[GPU_RASTER_VRAM_PIXELS];
static uint16_t g_check_end[GPU_RASTER_VRAM_PIXELS];
static GpItem g_check_items[GPU_ITEM_CAP];
static uint32_t g_check_payload[GPU_PAYLOAD_CAP];
static unsigned long g_check_frames, g_check_bad, g_check_px_total;
static size_t g_check_max_items, g_check_max_words;
static int g_check_registered;

static void replay_check_reset(void) {
    memset(g_check_start, 0, sizeof g_check_start);
}

static void replay_check_summary(void) {
    fprintf(stderr, "jojo-gpu: replay check: %lu frames, %lu with differences, %lu differing pixels in total\n",
            g_check_frames, g_check_bad, g_check_px_total);
    fprintf(stderr, "jojo-gpu: replay check: largest frame stream %lu entries of %u, %lu payload words of %u\n",
            (unsigned long)g_check_max_items, (unsigned)GPU_RASTER_MAX_ITEMS,
            (unsigned long)g_check_max_words, (unsigned)GPU_RASTER_MAX_PAYLOAD_WORDS);
}

static void replay_check(void) {
    size_t n_items = 0, n_words = 0;
    size_t i, diff = 0;
    int rc;
    GpuDisplayEnv disp;
    GpuSoftConfig cfg;
    if (g_check_on < 0) {
        const char *e = getenv("JOJO_GPU_REPLAY_CHECK");
        g_check_on = (e && *e && strcmp(e, "0") != 0) ? 1 : 0;
    }
    if (!g_check_on) return;
    if (!g_check_registered) { g_check_registered = 1; atexit(replay_check_summary); }
    rc = gpu_raster_copy_published_ordered(g_check_items, GPU_ITEM_CAP, &n_items, g_check_payload, GPU_PAYLOAD_CAP, &n_words,
                                           g_check_end, GPU_RASTER_VRAM_PIXELS, &disp, NULL);
    if (rc == 0) { fprintf(stderr, "jojo-gpu: replay check: published copy refused\n"); g_check_bad++; return; }
    if (n_items > g_check_max_items) g_check_max_items = n_items;
    if (n_words > g_check_max_words) g_check_max_words = n_words;
    memcpy(g_check_model.px, g_check_start, sizeof g_check_start);
    g_check_model.dirty_n = 0;
    cfg.disp_w = disp.w;
    cfg.disp_h = disp.h;
    if (gpu_raster_replay(&g_check_model, &cfg, g_check_items, n_items, g_check_payload, n_words, NULL, NULL) != 0) {
        fprintf(stderr, "jojo-gpu: replay check: replay rejected an entry (items=%lu)\n", (unsigned long)n_items);
        g_check_bad++;
    } else {
        const uint16_t *a = &g_check_model.px[0][0];
        for (i = 0; i < GPU_RASTER_VRAM_PIXELS; i++) if (a[i] != g_check_end[i]) diff++;
    }
    if (diff || rc != 1) {
        if (diff) { g_check_bad++; g_check_px_total += diff; }
        fprintf(stderr, "jojo-gpu: replay check frame %lu: %lu differing pixels, copy rc=%d\n",
                g_check_frames, (unsigned long)diff, rc);
    }
    memcpy(g_check_start, g_check_end, sizeof g_check_start);
    g_check_frames++;
}

/* Frame list of a dump switch: "a,b,c" or ranges "a-b" (inclusive). At most cap frames. */
static size_t parse_frame_list(const char *c, uint64_t *out, size_t cap) {
    size_t n = 0;
    while (*c && n < cap) {
        uint64_t a = strtoull(c, NULL, 0), b;
        while (*c && *c != ',' && *c != '-') c++;
        b = a;
        if (*c == '-') {
            c++;
            b = strtoull(c, NULL, 0);
            while (*c && *c != ',') c++;
        }
        for (; a <= b && n < cap; a++) out[n++] = a;
        if (*c == ',') c++;
    }
    return n;
}

/* Diagnostics, all written at the frame boundary of each listed frame (see gpu_raster_copy_prov / gpu_raster_copy_stream):
 *   JOJO_GPU_PROV_DUMP=<prefix>:<frames>   <prefix>.f<frame>.prov (u32 per VRAM pixel: entry index + 1 of the last writer in the
 *                                           open stream, 0 = none) and <prefix>.f<frame>.items
 *   JOJO_GPU_ITEMS_DUMP=<prefix>:<frames>  <prefix>.f<frame>.items only
 * .items has one text line per open stream entry (line number = entry index from 1). */
static uint32_t g_prov_dump[GPU_RASTER_VRAM_PIXELS];
static GpItem g_stream_dump[GPU_ITEM_CAP];

static void dump_items_text(const char *prefix, uint64_t frame) {
    char path[512];
    FILE *f;
    size_t k, cnt;
    snprintf(path, sizeof path, "%s.f%llu.items", prefix, (unsigned long long)frame);
    cnt = gpu_raster_copy_stream(g_stream_dump, GPU_ITEM_CAP);
    f = fopen(path, "w");
    if (!f) return;
    for (k = 0; k < cnt; k++) {
        const GpItem *it = &g_stream_dump[k];
        fprintf(f, "%zu type=%u flags=%u semi=%u tpage=%u clut=%u tw=%u area=%d,%d,%d,%d off=%d,%d wh=%u,%u "
                   "x=%d,%d,%d,%d y=%d,%d,%d,%d rgb0=%u,%u,%u rgb1=%u,%u,%u rgb2=%u,%u,%u rgb3=%u,%u,%u "
                   "uv0=%u,%u uv1=%u,%u uv2=%u,%u uv3=%u,%u payload=%u,%u\n",
                k + 1, it->type, it->flags, it->semi_mode, it->tpage, it->clut, it->tex_window,
                it->area_x0, it->area_y0, it->area_x1, it->area_y1, it->off_x, it->off_y, it->w, it->h,
                it->x[0], it->x[1], it->x[2], it->x[3], it->y[0], it->y[1], it->y[2], it->y[3],
                it->r[0], it->g[0], it->b[0], it->r[1], it->g[1], it->b[1], it->r[2], it->g[2], it->b[2],
                it->r[3], it->g[3], it->b[3], it->u[0], it->v[0], it->u[1], it->v[1], it->u[2], it->v[2],
                it->u[3], it->v[3], it->payload_off, it->payload_n);
    }
    fclose(f);
}

static void maybe_prov_dump(uint64_t frame) {
    static char prefix_p[400], prefix_i[400];
    static uint64_t frames_p[1024], frames_i[1024];
    static size_t n_p, n_i;
    static int parsed;
    size_t q;
    if (!parsed) {
        const char *e;
        parsed = 1;
        e = getenv("JOJO_GPU_PROV_DUMP");
        if (e && strchr(e, ':')) {
            size_t pl = (size_t)(strchr(e, ':') - e);
            if (pl >= sizeof prefix_p) pl = sizeof prefix_p - 1;
            memcpy(prefix_p, e, pl);
            prefix_p[pl] = 0;
            n_p = parse_frame_list(strchr(e, ':') + 1, frames_p, 1024);
        }
        e = getenv("JOJO_GPU_ITEMS_DUMP");
        if (e && strchr(e, ':')) {
            size_t pl = (size_t)(strchr(e, ':') - e);
            if (pl >= sizeof prefix_i) pl = sizeof prefix_i - 1;
            memcpy(prefix_i, e, pl);
            prefix_i[pl] = 0;
            n_i = parse_frame_list(strchr(e, ':') + 1, frames_i, 1024);
        }
    }
    for (q = 0; q < n_p; q++) {
        if (frames_p[q] != frame) continue;
        char path[512];
        FILE *f;
        snprintf(path, sizeof path, "%s.f%llu.prov", prefix_p, (unsigned long long)frame);
        if (gpu_raster_copy_prov(g_prov_dump, GPU_RASTER_VRAM_PIXELS)) {
            f = fopen(path, "wb");
            if (f) { fwrite(g_prov_dump, sizeof g_prov_dump[0], GPU_RASTER_VRAM_PIXELS, f); fclose(f); }
        }
        dump_items_text(prefix_p, frame);
        break;
    }
    for (q = 0; q < n_i; q++) {
        if (frames_i[q] != frame) continue;
        dump_items_text(prefix_i, frame);
        break;
    }
}

/* Called at the normal host frame boundary, after guest rendering has queued its GP0 packets. */
void jojo_gpu_frame(void) {
    if (!jojo_piece_on(JOJO_PIECE_GPU)) return;
    ensure_model();
    maybe_vram_dump(jojo_frame_now());
    maybe_prov_dump(jojo_frame_now());
    replay_check();
    shadow_compare(jojo_frame_now());
    gpu_raster_publish_frame();
}

/* A future GPUREAD/MMIO hook can drain the CPU-side readback synchronously through this function. */
void jojo_gpu_store_image(int x, int y, int w, int h, uint16_t *pixels) {
    ensure_model();
    gpu_raster_store(x, y, w, h, pixels);
}

void func_80035D20(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    jojo_note_call(0x80035d20, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x80035d20, jojo_lle_func_80035D20); TRACE4(0x80035d20); return; }
    /* ResetGraph's return value is the end of its mode-dependent LLE path (v0 = 5 in the reference), so the body runs as LLE;
     * the native GP0 model is reset first. */
    reset_gpu_model();
    LLE_CALL(0x80035d20, jojo_lle_func_80035D20); TRACE4(0x80035d20); return;
}

void func_80036004(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    jojo_note_call(0x80036004, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x80036004, jojo_lle_func_80036004); TRACE4(0x80036004); return; }
    /* The original returns the result of its GP1 write through the libgpu vector (v0 = 1 in the reference), so the GP1
     * write stays on the LLE body. The native display state is updated first; the LLE body then ends the call itself. */
    ensure_model(); g_display.enabled = (uint8_t)(a0 != 0); gpu_raster_set_display(&g_display);
    LLE_CALL(0x80036004, jojo_lle_func_80036004); TRACE4(0x80036004); return;
}

void func_8003609C(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    jojo_note_call(0x8003609c, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x8003609c, jojo_lle_func_8003609C); TRACE4(0x8003609c); return; }
    (void)a0; ensure_model(); cpu->gpr[2]=0; DONE(0x8003609c);
}

/* VSync(mode) at 0x8003C210 is NOT wrapped in Phase 1 (lead decision, option b): it stays the translated body, the frame-pacing
 * primitive of the main loop. Its exact replacement belongs to Phase 2. Contract, from the IDA text (fightout/c/8003c210.c,
 * 8003c388.c) and the RAM at frame 344: globals 0x8006163C (pointer to RCnt1 0x1F801110), 0x80061640 (last RCnt1 sample),
 * 0x80061644 (last target), 0x80061638 (pointer to GPUSTAT 0x1F801814), 0x80062780 (vsync count, advanced by the vblank IRQ).
 * mode < 0 returns the count; mode 1 returns v1 = (u16)(RCnt1 - sample) with no waits; otherwise two timed waits
 * (func_8003C388, budget (mode-1)<<15 and 32768, timeout prints "VSync: timeout"), two GPUSTAT reads, a field poll on LCF
 * (bit 31) when bit 22 is set, then the target store and the RCnt1 resample. The GPUSTAT reads are the only GPU device
 * accesses after boot. */

void func_800362B0(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    jojo_note_call(0x800362b0, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x800362b0, jojo_lle_func_800362B0); TRACE4(0x800362b0); return; }
    ensure_model();
    gpu_raster_fill((int16_t)psx_read_half(a0), (int16_t)psx_read_half(a0+2),
                    psx_read_half(a0+4), psx_read_half(a0+6), gpu_vram_color15(a1,a2,a3));
    { uint32_t w[3] = { 0x02000000u|((a3&0xffu)<<16)|((a2&0xffu)<<8)|(a1&0xffu),
                        ((uint32_t)psx_read_half(a0+2)<<16)|psx_read_half(a0), ((uint32_t)psx_read_half(a0+6)<<16)|psx_read_half(a0+4) };
      shadow_feed(w, 3); }
    cpu->gpr[2]=0; DONE(0x800362b0);
}

void func_80036348(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    uint32_t *words; size_t n, i; int w,h;
    jojo_note_call(0x80036348, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x80036348, jojo_lle_func_80036348); TRACE4(0x80036348); return; }
    ensure_model(); w=psx_read_half(a0+4); h=psx_read_half(a0+6); n=((size_t)w*(size_t)h+1u)/2u;
    if (w<=0 || h<=0 || w>GPU_VRAM_W || h>GPU_VRAM_H || n>GPU_PAYLOAD_CAP) unsupported("LoadImage dimensions",a0,a1,w,h);
    words=(uint32_t *)malloc(n*sizeof *words); if (!words) unsupported("LoadImage allocation",a0,a1,w,h);
    for (i=0;i<n;i++) words[i]=psx_read_word(a1+(uint32_t)(i*4u));
    gpu_raster_load_words((int16_t)psx_read_half(a0),(int16_t)psx_read_half(a0+2),w,h,words,0);
    { uint32_t hd[3] = { 0xA0000000u, ((uint32_t)psx_read_half(a0+2)<<16)|psx_read_half(a0), ((uint32_t)h<<16)|(uint32_t)w };
      shadow_feed(hd, 3); shadow_feed(words, n); }
    free(words); cpu->gpr[2]=0; DONE(0x80036348);
}

static void clear_ot(CPUState *cpu, uint32_t ot, unsigned count, int reverse) {
    unsigned i;
    if (!ot || !count || count>65536u) unsupported("ClearOTag size",ot,count,0,0);
    for (i=0;i<count;i++) {
        uint32_t next = reverse ? (i ? ot+4u*(i-1u) : 0x00ffffffu)
                                : (i+1u<count ? ot+4u*(i+1u) : 0x00ffffffu);
        /* The original (ClearOTag 0x800364c0, ClearOTagR 0x80036588) stores the link as a 24-bit address with the
         * count byte cleared. A KSEG0 link keeps 0x80 in the top byte, which the DMA walker reads as a packet count. */
        cpu->write_word(ot+4u*i, next & 0x00ffffffu);
    }
}

void func_800364C0(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    jojo_note_call(0x800364c0, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x800364c0, jojo_lle_func_800364C0); TRACE4(0x800364c0); return; }
    clear_ot(cpu,a0,a1,0); cpu->gpr[2]=a0; DONE(0x800364c0);
}

void func_80036588(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    jojo_note_call(0x80036588, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x80036588, jojo_lle_func_80036588); TRACE4(0x80036588); return; }
    clear_ot(cpu,a0,a1,1); cpu->gpr[2]=a0+(a1?4u*(a1-1u):0); DONE(0x80036588);
}

void func_80036634(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    uint32_t packet[256]; size_t n; unsigned i;
    jojo_note_call(0x80036634, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x80036634, jojo_lle_func_80036634); TRACE4(0x80036634); return; }
    ensure_model(); n=psx_read_byte(a0+3); if (n==0 || n>255) unsupported("DrawPrim length",a0,n,0,0);
    for(i=0;i<n;i++) packet[i]=psx_read_word(a0+4u+4u*i);
    decode_words(packet,n,a0); shadow_feed(packet,n); cpu->gpr[2]=0; DONE(0x80036634);
}

void func_80036690(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    GpOut out; int rc;
    jojo_note_call(0x80036690, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x80036690, jojo_lle_func_80036690); TRACE4(0x80036690); return; }
    ensure_model(); gp0_out_init(&out,g_items,GPU_ITEM_CAP,g_payload,GPU_PAYLOAD_CAP);
    rc=gp0_walk_ordering(read_guest_word,NULL,a0,GPU_OT_NODE_CAP,&g_gp_state,&out);
    if (rc!=GP_OK || out.unknown || out.truncated) {
        g_seen_unknown+=out.unknown;
        fprintf(stderr,"jojo-gpu: DrawOTag decode failed ot=%08X rc=%d unknown=%lu truncated=%lu\n",a0,rc,(unsigned long)out.unknown,(unsigned long)out.truncated);
        abort();
    }
    consume_items(&out,a0); shadow_feed_ot(a0); cpu->gpr[2]=0; DONE(0x80036690);
}

void func_80036700(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    unsigned twx,twy,tww,twh; GpState *s;
    jojo_note_call(0x80036700,cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x80036700,jojo_lle_func_80036700); TRACE4(0x80036700); return; }
    ensure_model(); s=&g_gp_state;
    s->area_x0=psx_read_half(a0); s->area_y0=psx_read_half(a0+2);
    s->area_x1=(uint16_t)(s->area_x0+psx_read_half(a0+4)-1u); s->area_y1=(uint16_t)(s->area_y0+psx_read_half(a0+6)-1u);
    s->off_x=(int16_t)psx_read_half(a0+8); s->off_y=(int16_t)psx_read_half(a0+10);
    twx=psx_read_half(a0+12); twy=psx_read_half(a0+14); tww=psx_read_half(a0+16); twh=psx_read_half(a0+18);
    s->tex_window=((twx>>3)&31u)|(((twy>>3)&31u)<<5)|(((~(tww>>3))&31u)<<10)|(((~(twh>>3))&31u)<<15);
    s->tpage=psx_read_half(a0+20);
    /* Keep state changes expressed through standard GP0 environment commands. */
    { uint32_t env[5]; GpOut out;
      env[0]=0xE3000000u|((uint32_t)s->area_x0&0x3ffu)|(((uint32_t)s->area_y0&0x1ffu)<<10);
      env[1]=0xE4000000u|((uint32_t)s->area_x1&0x3ffu)|(((uint32_t)s->area_y1&0x1ffu)<<10);
      env[2]=0xE5000000u|((uint16_t)s->off_x&0x7ffu)|(((uint32_t)(uint16_t)s->off_y&0x7ffu)<<11);
      env[3]=0xE2000000u|s->tex_window;
      /* E1 as sub_80037350: tpage bits 0..8 and 11, dtd (byte 22) to bit 9, dfe (byte 23) to bit 10. */
      env[4]=0xE1000000u|(s->tpage&0x9FFu)|((psx_read_byte(a0+22)&1u)<<9)|((psx_read_byte(a0+23)&1u)<<10);
      gp0_out_init(&out,g_items,GPU_ITEM_CAP,g_payload,GPU_PAYLOAD_CAP);
      if(gp0_decode_words(env,5,s,&out)!=GP_OK) unsupported("PutDrawEnv decode",a0,0,0,0);
      shadow_feed(env,5);
    }
    /* isbg (byte 24): the DR_ENV builder (IDA fightout/c/800370e0.c) appends a GP0 fill of the clip rect with colour
     * r0,g0,b0 (bytes 25..27). Fill position is the raw clip x,y (no draw offset); size is the clip w,h clamped to 0..1023 x 0..511
     * (VRAM limits at 0x8005C0F4/F6). Only the aligned case (x and w multiples of 64) is a fill; the unaligned case is a 0x60
     * rect draw in the original and is not reproduced (fails loud). */
    if (psx_read_byte(a0+24)) {
        int16_t cx=(int16_t)psx_read_half(a0), cy=(int16_t)psx_read_half(a0+2);
        int16_t cw=(int16_t)psx_read_half(a0+4), ch=(int16_t)psx_read_half(a0+6);
        int wc = (cw < 0) ? 0 : (cw > 1023 ? 1023 : cw);
        int hc = (ch < 0) ? 0 : (ch > 511 ? 511 : ch);
        if (((uint16_t)cx & 0x3Fu) != 0u || (wc & 0x3F) != 0) unsupported("PutDrawEnv isbg unaligned clip", a0, (uint32_t)cx, (uint32_t)wc, (uint32_t)hc);
        ensure_model();
        gpu_raster_fill(cx, cy, wc, hc, gpu_vram_color15(psx_read_byte(a0+25), psx_read_byte(a0+26), psx_read_byte(a0+27)));
        { uint32_t w[3] = { 0x02000000u|((uint32_t)psx_read_byte(a0+27)<<16)|((uint32_t)psx_read_byte(a0+26)<<8)|psx_read_byte(a0+25),
                            ((uint32_t)(uint16_t)cy<<16)|(uint16_t)cx, ((uint32_t)hc<<16)|(uint32_t)wc };
          shadow_feed(w, 3); }
    }
    (void)a1;(void)a2;(void)a3; cpu->gpr[2]=0; DONE(0x80036700);
}

void func_800368CC(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    jojo_note_call(0x800368cc,cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x800368cc,jojo_lle_func_800368CC); TRACE4(0x800368cc); return; }
    ensure_model(); g_display.x=psx_read_half(a0); g_display.y=psx_read_half(a0+2); g_display.w=psx_read_half(a0+4); g_display.h=psx_read_half(a0+6);
    g_display.interlace=psx_read_byte(a0+16); g_display.is_rgb24=psx_read_byte(a0+17); gpu_raster_set_display(&g_display);
    (void)a1;(void)a2;(void)a3; cpu->gpr[2]=0; DONE(0x800368cc);
}

void func_800389C0(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    int sx,sy,w,h,dx,dy;
    jojo_note_call(0x800389c0,cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x800389c0,jojo_lle_func_800389C0); TRACE4(0x800389c0); return; }
    ensure_model(); sx=(int16_t)psx_read_half(a0); sy=(int16_t)psx_read_half(a0+2); w=psx_read_half(a0+4); h=psx_read_half(a0+6); dx=(int16_t)a1; dy=(int16_t)a2;
    if (gpu_raster_copy(sx,sy,dx,dy,w,h,0)!=0) unsupported("MoveImage allocation",a0,a1,a2,a3);
    { uint32_t mw[4] = { 0x80000000u, ((uint32_t)(uint16_t)sy<<16)|(uint16_t)sx, ((uint32_t)(uint16_t)dy<<16)|(uint16_t)dx,
                         ((uint32_t)h<<16)|(uint32_t)w };
      shadow_feed(mw, 4); }
    cpu->gpr[2]=0; DONE(0x800389c0);
}

void func_800396F0(CPUState *cpu) {
    /* SetDefDrawEnv, as the LLE body (sub_800396F0): clip = (x, y, w, h); offset = (x, y); texture window zero; tpage 10;
     * dtd (byte 22) = 1; dfe (byte 23) = h below the NTSC limit of 257 (GetVideoMode 0); isbg and the trailing bytes zero.
     * Returns the env pointer in v0. */
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    uint32_t h=cpu->read_word(cpu->gpr[29]+16u);
    jojo_note_call(0x800396f0,cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x800396f0,jojo_lle_func_800396F0); TRACE4(0x800396f0); return; }
    cpu->write_half(a0,(uint16_t)a1); cpu->write_half(a0+2,(uint16_t)a2); cpu->write_half(a0+4,(uint16_t)a3); cpu->write_half(a0+6,(uint16_t)h);
    cpu->write_half(a0+8,(uint16_t)a1); cpu->write_half(a0+10,(uint16_t)a2);
    cpu->write_half(a0+12,0);cpu->write_half(a0+14,0);cpu->write_half(a0+16,0);cpu->write_half(a0+18,0);
    cpu->write_half(a0+20,10);
    cpu->write_byte(a0+22,1);
    cpu->write_byte(a0+23,(uint8_t)((h & 0xFFFFu) < 257u ? 1 : 0));
    cpu->write_byte(a0+24,0);cpu->write_byte(a0+25,0);cpu->write_byte(a0+26,0);cpu->write_byte(a0+27,0);
    cpu->gpr[2]=a0; DONE(0x800396f0);
}

void func_800397B0(CPUState *cpu) {
    /* SetDefDispEnv, as the LLE body (sub_800397B0): disp = (x, y, w, h); the screen rect and the two flag bytes 16..19 zero;
     * bytes 20..23 are not written. Returns the env pointer in v0. */
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    uint32_t h=cpu->read_word(cpu->gpr[29]+16u);
    jojo_note_call(0x800397b0,cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x800397b0,jojo_lle_func_800397B0); TRACE4(0x800397b0); return; }
    cpu->write_half(a0,(uint16_t)a1);cpu->write_half(a0+2,(uint16_t)a2);cpu->write_half(a0+4,(uint16_t)a3);cpu->write_half(a0+6,(uint16_t)h);
    cpu->write_half(a0+8,0);cpu->write_half(a0+10,0);cpu->write_half(a0+12,0);cpu->write_half(a0+14,0);
    cpu->write_byte(a0+16,0);cpu->write_byte(a0+17,0);cpu->write_byte(a0+18,0);cpu->write_byte(a0+19,0);
    cpu->gpr[2]=a0; DONE(0x800397b0);
}

void func_800397F0(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    jojo_note_call(0x800397f0,cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x800397f0,jojo_lle_func_800397F0); TRACE4(0x800397f0); return; }
    cpu->gpr[2]=((a0&3u)<<7)|((a1&3u)<<5)|((a3&0x100u)?0x10u:0u)|((a2>>6)&15u); DONE(0x800397f0);
}

void func_80039830(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    jojo_note_call(0x80039830,cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x80039830,jojo_lle_func_80039830); TRACE4(0x80039830); return; }
    cpu->gpr[2]=((a1&0x1ffu)<<6)|((a0>>4)&0x3fu); (void)a2;(void)a3; DONE(0x80039830);
}

void func_80039850(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    uint32_t head=cpu->read_word(a0), packet=cpu->read_word(a1), next=head&0x00ffffffu;
    jojo_note_call(0x80039850,cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x80039850,jojo_lle_func_80039850); TRACE4(0x80039850); return; }
    cpu->write_word(a1,(packet&0xff000000u)|next); cpu->write_word(a0,(head&0xff000000u)|(a1&0x00ffffffu));
    (void)a2;(void)a3; cpu->gpr[2]=0; DONE(0x80039850);
}

void func_80039890(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4], a1=cpu->gpr[5], a2=cpu->gpr[6], a3=cpu->gpr[7];
    uint32_t cmd;
    jojo_note_call(0x80039890,cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_GPU)) { LLE_CALL(0x80039890,jojo_lle_func_80039890); TRACE4(0x80039890); return; }
    cmd=cpu->read_word(a0+4); if(a1)cmd|=0x02000000u;else cmd&=~0x02000000u;cpu->write_word(a0+4,cmd);
    (void)a2;(void)a3;cpu->gpr[2]=0;DONE(0x80039890);
}

static void setup_primitive(CPUState *cpu,uint32_t prim,unsigned length,unsigned opcode) {
    uint32_t tag=cpu->read_word(prim), cmd=cpu->read_word(prim+4);
    cpu->write_word(prim,(tag&0x00ffffffu)|(length<<24));
    cpu->write_word(prim+4,(cmd&0x00ffffffu)|(opcode<<24));
}

#define PRIM_WRAPPER(fn,address,len,op) \
void fn(CPUState *cpu) { \
    uint32_t a0=cpu->gpr[4],a1=cpu->gpr[5],a2=cpu->gpr[6],a3=cpu->gpr[7]; \
    jojo_note_call(address,cpu->gpr[31]); \
    if(!jojo_piece_on(JOJO_PIECE_GPU)){LLE_CALL(address,jojo_lle_##fn);TRACE4(address);return;} \
    setup_primitive(cpu,a0,len,op); (void)a1;(void)a2;(void)a3;cpu->gpr[2]=0;DONE(address); \
}

PRIM_WRAPPER(func_800398C0,0x800398c0,9,0x2c)
PRIM_WRAPPER(func_800398E0,0x800398e0,8,0x38)
PRIM_WRAPPER(func_80039900,0x80039900,3,0x7c)
PRIM_WRAPPER(func_80039920,0x80039920,4,0x64)
PRIM_WRAPPER(func_80039940,0x80039940,3,0x60)
PRIM_WRAPPER(func_80039960,0x80039960,3,0x40)

void func_80039980(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4],a1=cpu->gpr[5],a2=cpu->gpr[6],a3=cpu->gpr[7];
    uint32_t e1=(a3&0x09ffu)|((a2!=0u)?0x0200u:0u)|((a1!=0u)?0x0400u:0u);
    jojo_note_call(0x80039980,cpu->gpr[31]);
    if(!jojo_piece_on(JOJO_PIECE_GPU)){LLE_CALL(0x80039980,jojo_lle_func_80039980);TRACE4(0x80039980);return;}
    setup_primitive(cpu,a0,1,0xe1);cpu->write_word(a0+4,0xe1000000u|e1);cpu->gpr[2]=e1;DONE(0x80039980);
}

void func_800399B0(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4],a1=cpu->gpr[5],a2=cpu->gpr[6],a3=cpu->gpr[7];
    uint32_t x=cpu->read_word(a0),y=cpu->read_word(a1); unsigned nx=x>>24,ny=y>>24;
    jojo_note_call(0x800399b0,cpu->gpr[31]);
    if(!jojo_piece_on(JOJO_PIECE_GPU)){LLE_CALL(0x800399b0,jojo_lle_func_800399B0);TRACE4(0x800399b0);return;}
    if(nx+ny>16u)cpu->gpr[2]=0;else {cpu->write_word(a0,(x&0x00ffffffu)|((nx+ny)<<24));cpu->gpr[2]=1;}
    (void)a2;(void)a3;DONE(0x800399b0);
}

void func_800399F0(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4],a1=cpu->gpr[5],a2=cpu->gpr[6],a3=cpu->gpr[7];
    uint32_t tw=cpu->read_word(cpu->gpr[29]+16u), e1, e2;
    unsigned x=psx_read_half(tw),y=psx_read_half(tw+2u),w=psx_read_half(tw+4u),h=psx_read_half(tw+6u);
    e1=0xe1000000u|(a3&0x09ffu)|((a2!=0u)?0x0200u:0u)|((a1!=0u)?0x0400u:0u);
    e2=0xe2000000u|((x>>3)&31u)|(((y>>3)&31u)<<5)|(((~(w>>3))&31u)<<10)|(((~(h>>3))&31u)<<15);
    jojo_note_call(0x800399f0,cpu->gpr[31]);
    if(!jojo_piece_on(JOJO_PIECE_GPU)){LLE_CALL(0x800399f0,jojo_lle_func_800399F0);TRACE4(0x800399f0);return;}
    setup_primitive(cpu,a0,2,0xe1);cpu->write_word(a0+4,e1);cpu->write_word(a0+8,e2);cpu->gpr[2]=e1;DONE(0x800399f0);
}

/* ResetCallback / StopCallback (0x8003c4a0, 0x8003c594) only call through the libgpu function vector at 0x80062740 (entries 3 and 4).
 * They do no GP0 or VRAM work, so they stay on the LLE body with the GPU piece on too. */
void func_8003C4A0(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4],a1=cpu->gpr[5],a2=cpu->gpr[6],a3=cpu->gpr[7];
    jojo_note_call(0x8003c4a0,cpu->gpr[31]);
    LLE_CALL(0x8003c4a0,jojo_lle_func_8003C4A0);TRACE4(0x8003c4a0);
}

void func_8003C594(CPUState *cpu) {
    uint32_t a0=cpu->gpr[4],a1=cpu->gpr[5],a2=cpu->gpr[6],a3=cpu->gpr[7];
    jojo_note_call(0x8003c594,cpu->gpr[31]);
    LLE_CALL(0x8003c594,jojo_lle_func_8003C594);TRACE4(0x8003c594);
}
