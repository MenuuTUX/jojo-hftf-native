/* Phase 1 libsnd/libspu adapter (hybrid stage, development only).
 *
 * The Capcom sound driver (exe functions around 0x80023860..0x80024100, and the
 * PAC loader at 0x80018480 and 0x8001C72C) calls libsnd and libspu through the 22
 * entries bound here (PHASE1_SURFACE.md, section libsnd; IDA pass in
 * ida/IDA_RESULTS.md). The three other rows of that section (0x8003EC00,
 * 0x80041018, 0x80042000) are data words in overlays, not call targets, and are
 * not bound.
 *
 * With JOJO_PIECE_AUDIO off every wrapper runs the original generated body
 * (jojo_lle_func_XXXXXXXX). With the piece on, each wrapper implements the
 * contract the Capcom code uses:
 *   - VAB heads are parsed from guest RAM (platform/audio_vab), sample bodies are
 *     copied from guest RAM when the game transfers them (SsVabTransBody and the
 *     partial form the PAC loader uses).
 *   - Voices are mixed by platform/audio_mix. Pitch, volume, pan and ADSR follow
 *     the library code of the exe (SsUtKeyOnV, SsVmKeyOnNow, sub_80040AE4), line
 *     by line, with the exe's own tables (below).
 *   - Writes that the library applies at its vblank flush (SsVmFlush) are queued
 *     and applied by jojo_audio_frame(), so SpuGetKeyStatus answers from the same
 *     applied state the Capcom code saw.
 *   - No SPU register is touched. SPU RAM layout and DMA are not reproduced:
 *     bodies live in host memory, SPU RAM addresses are only range checked.
 *
 * A case whose use is not pinned down, or whose result the native path does not
 * model, aborts with a message (jojo_pad.c style). Nothing is silently wrong.
 *
 * Hooks for the shell (the lead wires them):
 *   void     jojo_audio_frame(void)           once per game tick (60 Hz): applies
 *                                             the queued voice operations and
 *                                             renders 735 frames into a ring.
 *   size_t   jojo_audio_pull(int16_t *out, size_t frames)
 *                                             copies rendered stereo frames for the
 *                                             output device (not thread safe: call
 *                                             it from the thread that calls the
 *                                             frame hook, or lock it).
 *   int      jojo_audio_serial_volume(int serial, int *left, int *right)
 *                                             CD/XA input volume set by
 *                                             SsSetSerialVol (for the XA piece).
 */
#include "jojo_guest.h"
#include "jojo_core.h"
#include "cpu_state.h"
#include "psx_memory.h"
#include "audio_mix.h"
#include "audio_vab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint8_t *memory_get_ram_ptr(void);
extern uint8_t *memory_get_scratchpad_ptr(void);

#define LLE_DECL(addr) extern void jojo_lle_func_##addr(CPUState *cpu)
LLE_DECL(8003DEC0); LLE_DECL(8003DF70); LLE_DECL(8003E050); LLE_DECL(8003E070);
LLE_DECL(8003E130); LLE_DECL(8003E3B0); LLE_DECL(8003F4C0); LLE_DECL(8003F5D0);
LLE_DECL(8003F940); LLE_DECL(8003FA90); LLE_DECL(8003FDEC); LLE_DECL(8003FFAC);
LLE_DECL(80041EE0); LLE_DECL(80041EF0); LLE_DECL(80041F00); LLE_DECL(80041F10);
LLE_DECL(80041FC0); LLE_DECL(80042420); LLE_DECL(800424E0); LLE_DECL(80042670);
LLE_DECL(800440B0); LLE_DECL(80044200);

#ifndef JOJO_AUDIO_CYCLES
#define JOJO_AUDIO_CYCLES 0
#endif

#define AU_VOICES        24u
#define AU_BANKS         16u
#define AU_SPU_RAM       0x80000u
#define AU_OPS           256u
#define AU_RENDER_FRAMES 735u        /* 44100 / 60: one game tick of output */
#define AU_RING_FRAMES   8192u
#define AU_RESERVED_MASK 0u          /* dword_800627E0: no writer in the exe, always 0 */

/* Exe tables (SLUS_010.60, the file offsets are 0x800 + addr - 0x80010000):
 * kSemi = word_80062830[12] = 2^(n/12) * 0x8000, kFine = word_80062848[128] =
 * 2^(f/1536) * 0x8000. Read from the exe image, not recomputed. */
static const uint16_t kSemi[12] = {
    32768, 34716, 36780, 38967, 41285, 43740, 46340, 49096, 52015, 55108, 58385, 61857,
};
static const uint16_t kFine[128] = {
    32768, 32782, 32797, 32812, 32827, 32842, 32856, 32871, 32886, 32901, 32916, 32931, 32945, 32960, 32975, 32990,
    33005, 33020, 33035, 33050, 33065, 33080, 33094, 33109, 33124, 33139, 33154, 33169, 33184, 33199, 33214, 33229,
    33244, 33259, 33274, 33289, 33304, 33319, 33334, 33349, 33364, 33379, 33394, 33410, 33425, 33440, 33455, 33470,
    33485, 33500, 33515, 33530, 33546, 33561, 33576, 33591, 33606, 33621, 33636, 33652, 33667, 33682, 33697, 33712,
    33728, 33743, 33758, 33773, 33789, 33804, 33819, 33834, 33850, 33865, 33880, 33896, 33911, 33926, 33941, 33957,
    33972, 33987, 34003, 34018, 34033, 34049, 34064, 34080, 34095, 34110, 34126, 34141, 34157, 34172, 34187, 34203,
    34218, 34234, 34249, 34265, 34280, 34296, 34311, 34327, 34342, 34358, 34373, 34389, 34404, 34420, 34435, 34451,
    34466, 34482, 34497, 34513, 34528, 34544, 34560, 34575, 34591, 34606, 34622, 34638, 34653, 34669, 34685, 34700,
};

typedef struct {
    uint8_t  state;        /* as byte_80098560: 0 free, 1 body loaded, 2 head open */
    uint32_t gen;          /* bumped at every open and close; queued operations check it */
    uint32_t version;
    uint32_t sb_addr;      /* SPU address given at open (range check only) */
    uint32_t body_bytes;
    uint32_t body_done;    /* body bytes written so far */
    VabBank *head;
    uint8_t *body;
} AuBank;

typedef enum { AU_OP_KEY_ON, AU_OP_KEY_OFF, AU_OP_VOLUME } AuOpKind;

typedef struct {
    AuOpKind kind;
    uint8_t  voice;
    uint8_t  bank;
    uint32_t gen;
    uint32_t start;
    uint16_t adsr1, adsr2, pitch, vol_l, vol_r;
} AuOp;

static MixCtx  g_mix;
static AuBank  g_bank[AU_BANKS];
static AuOp    g_ops[AU_OPS];
static unsigned g_nops;
static uint8_t g_keyed[AU_VOICES];      /* SPU key bit as applied at the last flush (dword_8006296C) */
static int     g_voice_bank[AU_VOICES]; /* bank a voice plays, -1 none */
static int     g_running, g_mono, g_in_transfer, g_inited;
static int     g_partial_id = -1;       /* word_80062950 */
static uint32_t g_partial_left;         /* dword_80098538 */
static uint32_t g_xfer_mode;            /* dword_80062970 */
static int     g_serial_l[2], g_serial_r[2];
static int     g_reverb_noted;
static int16_t g_ring[AU_RING_FRAMES * 2];
static size_t  g_ring_r, g_ring_n;
static int16_t g_render[AU_RENDER_FRAMES * 2];

/* ---- guest memory and helpers ------------------------------------------- */

static uint8_t *guest_span(uint32_t addr, uint32_t n) {
    uint32_t phys = addr & 0x1fffffffu;
    uint32_t off;
    if (n == 0) return NULL;
    if (phys >= 0x1f800000u && phys < 0x1f800400u &&
        n <= 0x1f800400u - phys)
        return memory_get_scratchpad_ptr() + (phys - 0x1f800000u);
    if (!psx_ram_resolve(addr, n, &off)) return NULL;
    return memory_get_ram_ptr() + off;
}

static uint32_t rd16(const uint8_t *p) { return (uint32_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void unsupported(const char *fn, uint32_t a0, uint32_t a1, uint32_t a2) {
    fprintf(stderr, "jojo-audio: unsupported %s args a0=%08X a1=%08X a2=%08X\n", fn, a0, a1, a2);
    abort();
}

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

/* ---- transfer timing and per-call cost --------------------------------- */

/* Reference: the SPU DMA (channel 4) of a chunk is delayed by DMA_SPU_CYCLES_PER_WORD = 48 cycles per
 * word (runtime dma.c, schedule_delayed_complete). A transfer started by SsVabTransBodyPartly or
 * SsVabTransBody is in flight until jojo_cycles_now() reaches g_xfer_done; SsVabTransCompleted returns 0
 * until then. Measured in the reference: 284 to 286 polls per 2 KB chunk, all inside one frame. */
#define AU_XFER_CYCLES_PER_WORD 48u
static uint64_t g_xfer_done;
static int      g_xfer_pending;

static void xfer_start(uint32_t bytes) {
    g_xfer_done = jojo_cycles_now() + (uint64_t)((bytes + 3u) / 4u) * AU_XFER_CYCLES_PER_WORD;
    g_xfer_pending = 1;
}

/* Dispatch cost of a reference call that the body average above does not contain. Calibrated on the
 * SsVabTransCompleted poll loop: the reference spends 24576 / 285 = 86.2 cycles per poll (one 2 KB chunk
 * per ~285 polls); this native loop costs 5.9 cycles per iteration at a 11-cycle charge, so the reference
 * dispatch is about 86.2 - 5.9 - 11.2 = 69 cycles. GUESS: the same overhead applies to every wrapped call. */
#define AU_CALL_OVERHEAD 69u

/* Cycles charged per call: the mean cost of the original body in the reference run (report.json counts,
 * cycles avg, phase1-083). Functions the reference never called keep JOJO_AUDIO_CYCLES. */
static uint32_t cycles_of(uint32_t addr) {
    static const struct { uint32_t addr, cyc; } t[] = {
        { 0x8003DF70u, 60000u }, /* SsInit, GUESS: see the report (charge sensitivity: 340 at 40000, 341 at 60000) */
        { 0x8003E070u, 48u }, { 0x8003E130u, 31u }, { 0x8003E3B0u, 14u },
        { 0x8003F4C0u, 64u }, { 0x8003F5D0u, 50u }, { 0x8003F940u, 72u }, { 0x8003FA90u, 150u },
        { 0x8003FDECu, 24u }, { 0x80041EE0u, 10u }, { 0x80041F00u, 10u }, { 0x80041F10u, 48u },
        { 0x80041FC0u, 24u }, { 0x800424E0u, 65u }, { 0x80042670u, 11u }, { 0x800440B0u, 120u },
        { 0x80044200u, 27u },
    };
    unsigned i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) if (t[i].addr == addr) return t[i].cyc + AU_CALL_OVERHEAD;
    return JOJO_AUDIO_CYCLES;
}

/* ---- state -------------------------------------------------------------- */

static void bank_release(unsigned id) {
    AuBank *b = &g_bank[id];
    uint32_t gen = b->gen + 1u;
    free(b->head);
    free(b->body);
    memset(b, 0, sizeof *b);
    b->gen = gen;
}

static void voices_drop_bank(unsigned id) {
    unsigned v;
    for (v = 0; v < AU_VOICES; v++) {
        if (g_voice_bank[v] == (int)id) {
            g_mix.voice[v].active = 0;      /* the body is freed: stop the voice now */
            g_voice_bank[v] = -1;
            g_keyed[v] = 0;
        }
    }
}

static void audio_reset(void) {
    unsigned id, v;
    mix_init(&g_mix);
    mix_set_main_volume(&g_mix, 0, 0);
    for (id = 0; id < AU_BANKS; id++) bank_release(id);
    memset(g_ops, 0, sizeof g_ops);
    g_nops = 0;
    memset(g_keyed, 0, sizeof g_keyed);
    for (v = 0; v < AU_VOICES; v++) g_voice_bank[v] = -1;
    g_running = 0;
    g_mono = 0;
    g_in_transfer = 0;
    g_partial_id = -1;
    g_partial_left = 0;
    g_xfer_mode = 0;
    g_xfer_pending = 0;
    memset(g_serial_l, 0, sizeof g_serial_l);
    memset(g_serial_r, 0, sizeof g_serial_r);
    g_ring_r = g_ring_n = 0;
    g_inited = 1;
}

static void ensure_init(void) {
    if (!g_inited) audio_reset();
}

static void queue_op(const AuOp *op) {
    if (g_nops >= AU_OPS) unsupported("voice operation queue full", op->voice, op->bank, 0);
    g_ops[g_nops++] = *op;
}

/* ---- pitch and volume (exe code, line by line) -------------------------- */

/* sub_80040AE4(note, fine, center, shift) as reached from sub_80040A7C: the
 * caller passes tone+4 (center) and tone+5 (shift) in a2 and a3. Returns the
 * SPU pitch register value (0x1000 = 44.1 kHz). */
static uint16_t pitch_for(int32_t note, int32_t fine, uint8_t center, uint8_t shift) {
    int32_t v4 = (int16_t)(shift + fine);
    int32_t v5 = note + v4 / 128 - center;
    int32_t v7 = v4 % 128;
    int32_t v8 = v7;
    int32_t v6 = v5;
    int16_t s;
    int32_t v9, v10, v11, k;
    uint32_t prod, r;

    if (v7 < 0) {           /* bit 15 of v7 is set exactly when v7 < 0 */
        v8 = (int16_t)(v7 + 128);
        v6 = v5 - 1 + (int16_t)(v7 + 128) / 128;
    }
    s = (int16_t)v6;
    v9 = (int32_t)(((int64_t)715827883 * s) >> 32) >> 1;
    v9 -= s >> 15;
    v10 = (int16_t)(s / 12 - 2);
    v11 = s % 12;
    if (v11 < 0) {
        v11 += 12;
        v10 = (int16_t)(v9 - 3);
    }
    if (v10 >= 0) return 0x3FFFu;
    k = -v10;
    if (k < 1 || k > 30) unsupported("pitch shift", (uint32_t)note, (uint32_t)fine, (uint32_t)center);
    prod = (uint32_t)kSemi[v11] * (uint32_t)kFine[v8];
    r = ((prod >> 16) + (1u << (k - 1))) >> k;
    return (uint16_t)r;
}

/* SsUtKeyOnV volume and pan: volume = max(l, r), pan from the ratio (64 = centre). */
static void balance_split(int32_t l, int32_t r, uint32_t *vol, uint32_t *pan) {
    if (l == r) {
        *vol = (uint32_t)l;
        *pan = 64;
    } else if (r > l) {
        *vol = (uint32_t)r;
        *pan = (uint32_t)(127 - ((l << 6) / r));
    } else {
        *vol = (uint32_t)l;
        *pan = (uint32_t)((r << 6) / l);
    }
}

/* The library's (127 - x) and multiply are unsigned: mirror that. */
static uint32_t scale_up(uint32_t v, uint32_t x) { return v * (uint32_t)(127 - (int)x) / 0x3Fu; }
static uint32_t scale_dn(uint32_t v, uint32_t x) { return v * x / 0x3Fu; }

/* SsVmKeyOnNow levels for one voice. */
static void key_levels(const AuBank *b, const VabTone *t, unsigned prog, uint32_t vol, uint32_t bal_pan,
                       uint32_t *out_l, uint32_t *out_r) {
    const VabProg *pg = &b->head->prog[prog];
    uint32_t v3 = vol * 0x3FFFu * (uint32_t)b->head->mvol / 16129u
                  * (uint32_t)pg->vol * (uint32_t)t->vol / 0x3F01u;
    uint32_t l = v3, r = v3;
    if (t->pan >= 64) l = scale_up(l, t->pan); else r = scale_dn(r, t->pan);
    if (pg->pan >= 64) l = scale_up(l, pg->pan); else r = scale_dn(r, pg->pan);
    if (bal_pan >= 64) l = scale_up(l, bal_pan); else r = scale_dn(r, bal_pan);
    if (g_mono) {
        if (l >= r) r = l; else l = r;
    }
    *out_l = l;
    *out_r = r;
}

/* ---- VAB banks ---------------------------------------------------------- */

/* SsVabOpenHeadSticky(vh, vabid, sbadr). Returns the VAB id, or -1. */
static int32_t vab_open(uint32_t vh, int32_t vabid, uint32_t sbadr) {
    unsigned id;
    const uint8_t *hd, *sizes, *src;
    uint32_t version, ps, vs, tables_end, body_off, body_bytes, body_end, k;
    uint8_t *tmp;
    VabBank *head;
    AuBank *b;
    int rc;

    if (g_in_transfer == 1) return -1;
    g_in_transfer = 1;
    if (vabid >= (int32_t)AU_BANKS) {
        g_in_transfer = 0;
        return -1;
    }
    if (vabid < -1) unsupported("SsVabOpenHeadSticky vab id", (uint32_t)vabid, vh, sbadr);
    if (vabid == -1) {
        for (id = 0; id < AU_BANKS && g_bank[id].state != 0; id++) { }
        if (id == AU_BANKS) {
            g_in_transfer = 0;
            return -1;
        }
    } else {
        id = (unsigned)vabid;
        if (g_bank[id].state != 0) {
            g_in_transfer = 0;
            return -1;
        }
    }
    g_partial_left = 0;

    hd = guest_span(vh, 0x20u);
    if (!hd) unsupported("SsVabOpenHeadSticky head address", vh, (uint32_t)vabid, sbadr);
    if (hd[0] != 'p' || hd[1] != 'B' || hd[2] != 'A' || hd[3] != 'V') {
        g_in_transfer = 0;
        return -1;
    }
    version = rd32(hd + 4);
    ps = rd16(hd + 0x12);
    vs = rd16(hd + 0x16);
    if (version < 5) unsupported("SsVabOpenHeadSticky version below 5", version, vh, sbadr);
    if (ps > 128) {
        g_in_transfer = 0;
        return -1;
    }
    if (vs > 255) unsupported("SsVabOpenHeadSticky vag count", vs, vh, sbadr);

    tables_end = 0x820u + 512u * ps + 512u;
    body_off = (tables_end + 0x7FFu) & ~0x7FFu;
    sizes = guest_span(vh + 0x820u + 512u * ps, 512u);
    if (!sizes) unsupported("SsVabOpenHeadSticky size table", vh, ps, vs);
    if (rd16(sizes) != 0) unsupported("SsVabOpenHeadSticky size table entry 0", vh, ps, vs);
    body_bytes = 0;
    for (k = 1; k <= vs; k++) body_bytes += 8u * rd16(sizes + 2u * k);
    body_end = body_off + body_bytes;
    if (sbadr + ((body_bytes + 63u) & ~63u) > AU_SPU_RAM) {   /* library: v34 + v33 > 0x80000 */
        g_in_transfer = 0;
        return -1;
    }

    src = guest_span(vh, tables_end);
    if (!src) unsupported("SsVabOpenHeadSticky tables", vh, ps, vs);
    tmp = (uint8_t *)calloc(1, body_end);
    if (!tmp) unsupported("SsVabOpenHeadSticky out of memory", body_end, vh, sbadr);
    memcpy(tmp, src, tables_end);       /* the body is not in guest RAM yet: zeros for vab_parse */
    head = (VabBank *)malloc(sizeof *head);
    rc = head ? vab_parse(head, tmp, body_end) : VAB_ERR_ARG;
    free(tmp);
    if (rc != VAB_OK) {
        free(head);
        unsupported("SsVabOpenHeadSticky vab_parse", (uint32_t)rc, vh, sbadr);
    }

    b = &g_bank[id];
    b->gen += 1u;
    b->version = version;
    b->sb_addr = sbadr;
    b->body_bytes = body_bytes;
    b->body_done = 0;
    b->head = head;
    b->body = (uint8_t *)malloc(body_bytes ? body_bytes : 1u);
    if (!b->body) unsupported("SsVabOpenHeadSticky body allocation", body_bytes, vh, sbadr);
    b->state = 2;
    return (int32_t)id;     /* the library leaves the in-transfer flag set until the body is in */
}

/* SsVabTransBody(vb, vabid): whole body from guest RAM. */
static int32_t vab_trans_body(uint32_t vb, uint32_t vabid) {
    AuBank *b;
    const uint8_t *src;
    if (vabid >= AU_BANKS || g_bank[vabid].state != 2) {
        g_in_transfer = 0;
        return -1;
    }
    b = &g_bank[vabid];
    g_xfer_mode = 0;                      /* SpuSetTransferMode(0), as the library call does */
    if (b->body_bytes) {
        src = guest_span(vb, b->body_bytes);
        if (!src) unsupported("SsVabTransBody source", vb, vabid, b->body_bytes);
        memcpy(b->body, src, b->body_bytes);
    }
    if (b->body_bytes) xfer_start(b->body_bytes);
    b->body_done = b->body_bytes;
    b->state = 1;
    g_in_transfer = 0;
    return (int32_t)vabid;
}

/* SsVabTransBodyPartly(src, size, vabid): streams in chunks; -2 until complete. */
static int32_t vab_trans_part(uint32_t src, uint32_t size, int32_t vabid) {
    AuBank *b;
    uint32_t n;
    if (vabid < 0 || vabid >= (int32_t)AU_BANKS || g_bank[vabid].state != 2) {
        g_in_transfer = 0;
        return -1;
    }
    b = &g_bank[vabid];
    if (g_partial_left == 0) {            /* first chunk of a new transfer */
        g_partial_left = b->body_bytes;
        g_partial_id = vabid;
        b->body_done = 0;
        g_xfer_mode = 0;
    }
    if (g_partial_id != vabid) {
        g_in_transfer = 0;
        return -1;
    }
    n = size < g_partial_left ? size : g_partial_left;
    g_in_transfer = 1;
    if (n) {
        const uint8_t *p = guest_span(src, n);
        if (!p) unsupported("SsVabTransBodyPartly source", src, n, (uint32_t)vabid);
        memcpy(b->body + b->body_done, p, n);
    }
    if (n) xfer_start(n);
    b->body_done += n;
    g_partial_left -= n;
    if (g_partial_left == 0) {
        g_partial_id = -1;
        b->state = 1;
        g_in_transfer = 0;
        return vabid;
    }
    return -2;
}

/* SsVabClose(vabid). Voices that still play the bank are stopped (GUESS, see report). */
static void vab_close(uint32_t vabid) {
    AuBank *b;
    if (vabid >= AU_BANKS) return;
    b = &g_bank[vabid];
    if (b->state == 0 || b->state >= 3) return;
    voices_drop_bank(vabid);
    if (g_partial_id == (int)vabid) {
        g_partial_id = -1;
        g_partial_left = 0;
    }
    if (g_in_transfer == 1) g_in_transfer = 0;
    bank_release(vabid);
}

/* SsUtKeyOnV: parse the tone, compute the register values, queue the key-on.
 * Returns the voice, or -1 when the library would refuse (vag 0, bad VAB). */
static int32_t key_on_native(int32_t vc, int32_t vab, int32_t prog, int32_t tone, int32_t note,
                             int32_t fine, int32_t voll, int32_t volr) {
    AuBank *b;
    const VabTone *t;
    uint32_t grp = 0, vol, pan, l, r, start;
    unsigned p;
    AuOp op;
    int32_t vag;

    if ((uint16_t)vc >= AU_VOICES) return -1;              /* (unsigned __int16)a1 >= 0x18 */
    if ((uint16_t)vab >= AU_BANKS || g_bank[vab].state != 1) return -1;   /* SsVmVSetUp */
    b = &g_bank[vab];
    if (prog < 0) unsupported("SsUtKeyOnV program", (uint32_t)prog, (uint32_t)vab, (uint32_t)vc);
    if (prog >= 128) return -1;                              /* word_80098522 */
    for (p = 0; p < (unsigned)prog; p++) {
        if (b->head->prog[p].tones) grp++;                   /* program record +8 */
    }
    if (tone < 0 || tone >= 16) unsupported("SsUtKeyOnV tone", (uint32_t)tone, (uint32_t)prog, (uint32_t)vab);
    t = &b->head->tone[16u * grp + (unsigned)tone];
    vag = t->vag;
    if (vag == 0) return -1;
    if (vag == 255) unsupported("SsUtKeyOnV noise voice (vag 255)", (uint32_t)vab, (uint32_t)prog, (uint32_t)tone);
    if (vag < 0 || (uint32_t)vag > b->head->vs) unsupported("SsUtKeyOnV vag", (uint32_t)vag, (uint32_t)vab, (uint32_t)prog);
    if (t->mode & 4) unsupported("SsUtKeyOnV reverb voice (tone mode bit 2): reverb is not modeled",
                                 (uint32_t)vab, (uint32_t)prog, (uint32_t)tone);
    if (voll < 0 || volr < 0 || voll > 255 || volr > 255)
        unsupported("SsUtKeyOnV volume", (uint32_t)voll, (uint32_t)volr, (uint32_t)vc);
    start = b->head->vag_off[vag] - b->head->body_off;
    if (start >= b->body_bytes) unsupported("SsUtKeyOnV sample start", start, (uint32_t)vab, (uint32_t)vag);

    balance_split(voll, volr, &vol, &pan);
    key_levels(b, t, (unsigned)prog, vol, pan, &l, &r);
    if (l > 0x7FFFu || r > 0x7FFFu) unsupported("SsUtKeyOnV volume register (sweep)", l, r, (uint32_t)vc);

    memset(&op, 0, sizeof op);
    op.kind = AU_OP_KEY_ON;
    op.voice = (uint8_t)vc;
    op.bank = (uint8_t)vab;
    op.gen = b->gen;
    op.start = start;
    op.adsr1 = t->adsr1;
    op.adsr2 = t->adsr2;             /* word_800984D8 is 0 on every path, so adsr2 is unchanged */
    op.pitch = pitch_for(note, fine, t->center, t->shift);
    op.vol_l = (uint16_t)l;
    op.vol_r = (uint16_t)r;
    queue_op(&op);
    return vc;
}

/* SpuGetKeyStatus(mask): lowest voice in the mask. 1 keyed and sounding, 3 keyed with
 * envelope 0, 2 released and still sounding, 0 off, -1 no voice. */
static int32_t key_status(uint32_t mask) {
    int voice = -1;
    unsigned v;
    int env_on;
    for (v = 0; v < AU_VOICES; v++) {
        if (mask & (1u << v)) {
            voice = (int)v;
            break;
        }
    }
    if (voice < 0) return -1;
    /* A voice that ran into END without REPEAT is off: the mixer marks it inactive and
     * its level no longer counts (GUESS: the SPU switches such a voice off). */
    env_on = g_mix.voice[voice].active && g_mix.voice[voice].env != 0;
    if (g_keyed[voice]) return env_on ? 1 : 3;
    return env_on ? 2 : 0;
}

/* ---- per-tick flush and output ------------------------------------------ */

static unsigned g_ticks;          /* game frames rendered (one per jojo_audio_frame call) */
/* Development log: JOJO_AUDIO_LOG=1 prints every key-on applied at a flush (tick = game frame). */
static int g_kon_log = -1;
static int kon_log_on(void) {
    if (g_kon_log < 0) g_kon_log = getenv("JOJO_AUDIO_LOG") && *getenv("JOJO_AUDIO_LOG") && strcmp(getenv("JOJO_AUDIO_LOG"), "0") != 0;
    return g_kon_log;
}

static void apply_ops(void) {
    unsigned i;
    for (i = 0; i < g_nops; i++) {
        const AuOp *op = &g_ops[i];
        switch (op->kind) {
        case AU_OP_KEY_ON: {
            AuBank *b = &g_bank[op->bank];
            int rc;
            if (b->gen != op->gen || b->state != 1) break;   /* bank closed or replaced since the call */
            if (kon_log_on())
                fprintf(stderr, "jojo-audio kon tick=%u voice=%u pitch=%u start=%u vol=%u,%u adsr=%04X,%04X\n",
                        g_ticks, op->voice, op->pitch, op->start, op->vol_l, op->vol_r, op->adsr1, op->adsr2);
            rc = mix_key_on(&g_mix, op->voice, b->body, b->body_bytes, op->start, op->start,
                            op->adsr1, op->adsr2, op->pitch, op->vol_l, op->vol_r);
            if (rc != 0) unsupported("mix_key_on", (uint32_t)op->voice, op->start, (uint32_t)rc);
            g_keyed[op->voice] = 1;
            g_voice_bank[op->voice] = op->bank;
            break;
        }
        case AU_OP_KEY_OFF:
            mix_key_off(&g_mix, op->voice);
            g_keyed[op->voice] = 0;
            break;
        case AU_OP_VOLUME:
            if (mix_set_volume(&g_mix, op->voice, op->vol_l, op->vol_r) != 0)
                unsupported("mix_set_volume", (uint32_t)op->voice, op->vol_l, op->vol_r);
            break;
        }
    }
    g_nops = 0;
}

static void ring_push(const int16_t *s, size_t frames) {
    size_t i;
    for (i = 0; i < frames; i++) {
        size_t w = (g_ring_r + g_ring_n) % AU_RING_FRAMES;
        if (g_ring_n == AU_RING_FRAMES) {          /* full: drop the oldest frame */
            g_ring_r = (g_ring_r + 1) % AU_RING_FRAMES;
            g_ring_n--;
            w = (g_ring_r + g_ring_n) % AU_RING_FRAMES;
        }
        g_ring[2 * w] = s[2 * i];
        g_ring[2 * w + 1] = s[2 * i + 1];
        g_ring_n++;
    }
}

/* Development capture: JOJO_AUDIO_DUMP=<path> writes every rendered tick as raw
 * interleaved int16 stereo at 44100 Hz (little endian, no header), up to
 * AU_DUMP_MAX_TICKS (60 s). Off when the variable is unset. */
#define AU_DUMP_MAX_TICKS 6000u   /* 100 s at 60 ticks/s */
static FILE *g_dump;
static unsigned g_dump_ticks;
static int g_dump_tried;

static void dump_tick(const int16_t *s, size_t frames) {
    if (!g_dump_tried) {
        const char *path = getenv("JOJO_AUDIO_DUMP");
        g_dump_tried = 1;
        if (path && *path) g_dump = fopen(path, "wb");
        if (!g_dump && path && *path) fprintf(stderr, "jojo-audio: cannot open dump %s\n", path);
    }
    if (!g_dump || g_dump_ticks >= AU_DUMP_MAX_TICKS) return;
    if (g_dump_ticks == 0) fprintf(stderr, "jojo-audio dump start tick=%u\n", g_ticks);
    fwrite(s, sizeof(int16_t), frames * 2u, g_dump);
    fflush(g_dump);
    g_dump_ticks++;
}

/* Per-tick hook (lead wires it from the frame clock). */
void jojo_audio_frame(void) {
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) return;
    ensure_init();
    if (!g_running) return;               /* SsEnd removes the library's vblank flush */
    apply_ops();
    mix_render(&g_mix, g_render, AU_RENDER_FRAMES);
    ring_push(g_render, AU_RENDER_FRAMES);
    dump_tick(g_render, AU_RENDER_FRAMES);
    g_ticks++;
}

size_t jojo_audio_pull(int16_t *out, size_t frames) {
    size_t n = frames < g_ring_n ? frames : g_ring_n;
    size_t i;
    for (i = 0; i < n; i++) {
        out[2 * i] = g_ring[2 * g_ring_r];
        out[2 * i + 1] = g_ring[2 * g_ring_r + 1];
        g_ring_r = (g_ring_r + 1) % AU_RING_FRAMES;
    }
    g_ring_n -= n;
    for (; i < frames; i++) {
        out[2 * i] = 0;
        out[2 * i + 1] = 0;
    }
    return n;
}

/* CD bus hooks for the XA stream (jojo_cd.c). Frames are 44,100 Hz stereo int16 after decode, resample and CD matrix.
 * JOJO_AUDIO_CDDUMP=<path> keeps a raw copy of every pushed frame (development capture, same cap as the tick dump). */
static FILE *g_cddump;
static int   g_cddump_tried;
static size_t g_cddump_frames;
void jojo_audio_cd_push(const int16_t *stereo, size_t frames) {
    ensure_init();
    if (!g_cddump_tried) {
        const char *path = getenv("JOJO_AUDIO_CDDUMP");
        g_cddump_tried = 1;
        if (path && *path) g_cddump = fopen(path, "wb");
    }
    if (g_cddump && g_cddump_frames < (size_t)AU_DUMP_MAX_TICKS * AU_RENDER_FRAMES) {
        fwrite(stereo, sizeof(int16_t), frames * 2u, g_cddump);
        g_cddump_frames += frames;
    }
    mix_cd_push(&g_mix, stereo, frames);
}

size_t jojo_audio_cd_pending(void) {
    return mix_cd_pending(&g_mix);
}

int jojo_audio_serial_volume(int serial, int *left, int *right) {
    if (serial < 0 || serial > 1 || !left || !right) return -1;
    *left = g_serial_l[serial];
    *right = g_serial_r[serial];
    return 0;
}

/* ---- guest entries ------------------------------------------------------ */

/* SsEnd: stops the vblank flush. Returns 127 as the library does. */
void func_8003DEC0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003DEC0);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x8003DEC0, jojo_lle_func_8003DEC0);
    ensure_init();
    g_running = 0;
    cpu->gpr[2] = 127;
    jojo_cycles_charge(cycles_of(0x8003DEC0u)); DONE(0x8003DEC0);
}

/* SsInit (SpuInit, VM init): everything the adapter holds is reset. */
void func_8003DF70(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003DF70);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x8003DF70, jojo_lle_func_8003DF70);
    audio_reset();
    cpu->gpr[2] = 0;
    jojo_cycles_charge(cycles_of(0x8003DF70u)); DONE(0x8003DF70);
}

/* SsQuit (SpuQuit) */
void func_8003E050(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003E050);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x8003E050, jojo_lle_func_8003E050);
    ensure_init();
    g_running = 0;
    cpu->gpr[2] = 0;
    jojo_cycles_charge(cycles_of(0x8003E050u)); DONE(0x8003E050);
}

/* SsSetSerialAttr(serial, attr, on): only serial 0/1, attr 0/1 exist in the library. */
void func_8003E070(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003E070);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x8003E070, jojo_lle_func_8003E070);
    ensure_init();
    if (a0 > 1 || a1 > 1) unsupported("SsSetSerialAttr", a0, a1, a2);
    if ((int32_t)a2 != 0 && !g_reverb_noted) {
        g_reverb_noted = 1;
        fprintf(stderr, "jojo-audio: note: SsSetSerialAttr(%u,%u,%d) enables SPU reverb on serial input; "
                        "reverb is not modeled in the native path\n", a0, a1, (int)a2);
    }
    cpu->gpr[2] = 0;
    jojo_cycles_charge(cycles_of(0x8003E070u)); DONE(0x8003E070);
}

/* SsSetMVol(left, right): main volume register = 129 * level. The register must stay
 * below bit 15 (sweep mode), so levels 0..254 are the valid range. */
void func_8003E130(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003E130);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x8003E130, jojo_lle_func_8003E130);
    ensure_init();
    if ((int16_t)a0 < 0 || (int16_t)a0 > 254 || (int16_t)a1 < 0 || (int16_t)a1 > 254)
        unsupported("SsSetMVol level", a0, a1, 0);
    mix_set_main_volume(&g_mix, (uint16_t)(129 * (int16_t)a0), (uint16_t)(129 * (int16_t)a1));
    cpu->gpr[2] = 0;
    jojo_cycles_charge(cycles_of(0x8003E130u)); DONE(0x8003E130);
}

/* SsStart: the vblank flush runs from here on. */
void func_8003E3B0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003E3B0);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x8003E3B0, jojo_lle_func_8003E3B0);
    ensure_init();
    g_running = 1;
    jojo_cycles_charge(cycles_of(0x8003E3B0u)); DONE(0x8003E3B0);
}

/* SsSetSerialVol(serial, left, right): CD/XA input volume, kept for the XA piece. */
void func_8003F4C0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003F4C0);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x8003F4C0, jojo_lle_func_8003F4C0);
    ensure_init();
    if (a0 > 1) unsupported("SsSetSerialVol serial", a0, a1, a2);
    if ((int16_t)a1 < 0 || (int16_t)a2 < 0) unsupported("SsSetSerialVol level", a1, a2, a0);
    g_serial_l[a0] = (int16_t)a1 >= 128 ? 127 : (int16_t)a1;
    g_serial_r[a0] = (int16_t)a2 >= 128 ? 127 : (int16_t)a2;
    if (a0 == 0) {
        /* Serial 0 is the CD input. The exe writes 258 * level to the CD volume (SsSetSerialVol, IDA). */
        mix_set_cd_gain(&g_mix, (int16_t)(258 * g_serial_l[0]), (int16_t)(258 * g_serial_r[0]));
    }
    cpu->gpr[2] = 0;
    jojo_cycles_charge(cycles_of(0x8003F4C0u)); DONE(0x8003F4C0);
}

/* SsSetTickMode(1): the game runs at 60 Hz. Other modes are not used. */
void func_8003F5D0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003F5D0);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x8003F5D0, jojo_lle_func_8003F5D0);
    if (a0 != 1) unsupported("SsSetTickMode", a0, a1, a2);
    cpu->gpr[2] = 60;       /* GUESS: the library returns the tick rate for an NTSC video mode */
    jojo_cycles_charge(cycles_of(0x8003F5D0u)); DONE(0x8003F5D0);
}

/* SsUtAllKeyOff: every voice goes to volume 0 now and is keyed off at the next
 * flush. The library ignores its argument. */
void func_8003F940(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003F940);
    unsigned v;
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x8003F940, jojo_lle_func_8003F940);
    ensure_init();
    for (v = 0; v < AU_VOICES; v++) {
        AuOp op;
        if (AU_RESERVED_MASK & (1u << v)) continue;
        mix_set_volume(&g_mix, v, 0, 0);
        memset(&op, 0, sizeof op);
        op.kind = AU_OP_KEY_OFF;
        op.voice = (uint8_t)v;
        queue_op(&op);
    }
    cpu->gpr[2] = AU_VOICES << 16;
    jojo_cycles_charge(cycles_of(0x8003F940u)); DONE(0x8003F940);
}

/* SsUtKeyOnV(vc, vab, prog, tone, note, fine, voll, volr): the last four arguments are on
 * the stack (16..28 of the caller's frame). */
void func_8003FA90(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003FA90);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x8003FA90, jojo_lle_func_8003FA90);
    {
        uint32_t sp = cpu->gpr[29];
        const uint8_t *s = guest_span(sp + 16u, 16u);
        int32_t rc;
        if (!s) unsupported("SsUtKeyOnV stack arguments", sp, a0, a1);
        ensure_init();
        rc = key_on_native((int16_t)a0, (int16_t)a1, (int16_t)a2, (int8_t)a3,
                           (int16_t)rd16(s), (int16_t)rd16(s + 4),
                           (int16_t)rd16(s + 8), (int16_t)rd16(s + 12));
        cpu->gpr[2] = (uint32_t)rc;
    }
    jojo_cycles_charge(cycles_of(0x8003FA90u)); DONE(0x8003FA90);
}

/* SsUtKeyOffV(vc): key off now (applied at the next flush). */
void func_8003FDEC(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003FDEC);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x8003FDEC, jojo_lle_func_8003FDEC);
    ensure_init();
    if ((uint16_t)a0 >= AU_VOICES) {
        cpu->gpr[2] = (uint32_t)-1;
    } else {
        AuOp op;
        memset(&op, 0, sizeof op);
        op.kind = AU_OP_KEY_OFF;
        op.voice = (uint8_t)a0;
        queue_op(&op);
        cpu->gpr[2] = 0;
    }
    jojo_cycles_charge(cycles_of(0x8003FDECu)); DONE(0x8003FDEC);
}

/* SsUtSetVVol(vc, voll, volr): the voice's L/R registers become 129 * level (applied at the flush). */
void func_8003FFAC(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x8003FFAC);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x8003FFAC, jojo_lle_func_8003FFAC);
    ensure_init();
    if ((uint16_t)a0 >= AU_VOICES) {
        cpu->gpr[2] = (uint32_t)-1;
    } else {
        AuOp op;
        if ((int16_t)a1 < 0 || (int16_t)a1 > 254 || (int16_t)a2 < 0 || (int16_t)a2 > 254)
            unsupported("SsUtSetVVol level", a1, a2, a0);
        memset(&op, 0, sizeof op);
        op.kind = AU_OP_VOLUME;
        op.voice = (uint8_t)a0;
        op.vol_l = (uint16_t)(129 * (int16_t)a1);
        op.vol_r = (uint16_t)(129 * (int16_t)a2);
        queue_op(&op);
        cpu->gpr[2] = 0;
    }
    jojo_cycles_charge(cycles_of(0x8003FFACu)); DONE(0x8003FFAC);
}

/* SsSetAutoKeyOffMode(0): the only mode the game uses. */
void func_80041EE0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80041EE0);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x80041EE0, jojo_lle_func_80041EE0);
    if (a0 != 0) unsupported("SsSetAutoKeyOffMode", a0, a1, a2);
    cpu->gpr[2] = 0;
    jojo_cycles_charge(cycles_of(0x80041EE0u)); DONE(0x80041EE0);
}

/* SsSetMono */
void func_80041EF0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80041EF0);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x80041EF0, jojo_lle_func_80041EF0);
    ensure_init();
    g_mono = 1;
    jojo_cycles_charge(cycles_of(0x80041EF0u)); DONE(0x80041EF0);
}

/* SsSetStereo */
void func_80041F00(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80041F00);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x80041F00, jojo_lle_func_80041F00);
    ensure_init();
    g_mono = 0;
    jojo_cycles_charge(cycles_of(0x80041F00u)); DONE(0x80041F00);
}

/* SsVabClose(vabid) */
void func_80041F10(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80041F10);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x80041F10, jojo_lle_func_80041F10);
    ensure_init();
    vab_close((uint16_t)a0);
    cpu->gpr[2] = 0;
    jojo_cycles_charge(cycles_of(0x80041F10u)); DONE(0x80041F10);
}

/* SsVabOpenHeadSticky(vh, vabid, sbadr) */
void func_80041FC0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80041FC0);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x80041FC0, jojo_lle_func_80041FC0);
    ensure_init();
    cpu->gpr[2] = (uint32_t)vab_open(a0, (int16_t)a1, a2);
    jojo_cycles_charge(cycles_of(0x80041FC0u)); DONE(0x80041FC0);
}

/* SsVabTransBody(vb, vabid) */
void func_80042420(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80042420);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x80042420, jojo_lle_func_80042420);
    ensure_init();
    cpu->gpr[2] = (uint32_t)vab_trans_body(a0, (uint16_t)a1);
    jojo_cycles_charge(cycles_of(0x80042420u)); DONE(0x80042420);
}

/* SsVabTransBodyPartly(src, size, vabid) */
void func_800424E0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x800424E0);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x800424E0, jojo_lle_func_800424E0);
    ensure_init();
    cpu->gpr[2] = (uint32_t)vab_trans_part(a0, a1, (int16_t)a2);
    jojo_cycles_charge(cycles_of(0x800424E0u)); DONE(0x800424E0);
}

/* SsVabTransCompleted: transfers are complete when the call returns. */
void func_80042670(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80042670);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x80042670, jojo_lle_func_80042670);
    if (g_xfer_pending && jojo_cycles_now() < g_xfer_done) {
        cpu->gpr[2] = 0;                 /* DMA still in flight: the caller polls again */
    } else {
        g_xfer_pending = 0;
        cpu->gpr[2] = 1;
    }
    jojo_cycles_charge(cycles_of(0x80042670u)); DONE(0x80042670);
}

/* SpuGetKeyStatus(mask) */
void func_800440B0(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x800440B0);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x800440B0, jojo_lle_func_800440B0);
    ensure_init();
    cpu->gpr[2] = (uint32_t)key_status(a0);
    jojo_cycles_charge(cycles_of(0x800440B0u)); DONE(0x800440B0);
}

/* SpuSetTransferMode(mode): stored; the native copy has one transfer path. */
void func_80044200(CPUState *cpu) {
    DECLARE_ARGS(); NOTE(0x80044200);
    if (!jojo_piece_on(JOJO_PIECE_AUDIO)) FALLBACK(0x80044200, jojo_lle_func_80044200);
    ensure_init();
    g_xfer_mode = a0;
    cpu->gpr[2] = (a0 == 1);
    jojo_cycles_charge(cycles_of(0x80044200u)); DONE(0x80044200);
}
