/* Phase 1 hybrid adapter for the libmcrd (memory card) family and the libmcx (PocketStation) family used by the game.
 *
 * Guest pointers, CPUState and callback storage stay in this development-only layer. The platform save module receives ordinary
 * byte buffers and fixed save-file ids (platform/save.c). Asynchronous operations keep the frame timing of the original: the
 * card operation is accepted at once, Capcom polls its state word, and the completion is delivered from jojo_saves_frame_tick
 * (once per vblank) as a call to the Capcom callback with (op, code). The latencies below are measured from REFERENCE runs
 * (issue frame to callback frame, see PHASE1 notes in the saves run directories).
 *
 * Save-file layout measured from the code (SLUS_010.60): 0x80055F30 = "BASLUSP01060-JOJOD", written with 8192 bytes (one card
 * block); 0x80055F44 = "BASLUSP01060-JOJOX", written with 106496 bytes (13 card blocks).
 *
 * Callback contract (Capcom 0x80015598): a0 = operation id (1 exist, 2 accept, 4 write, 5 read), a1 = result code; the Capcom job
 * struct at 0x80077E98 takes op at +8 and code at +12 (code 0 is stored as 255). Operation ids and codes are the libmcrd ones:
 * success after a read or write is code 0.
 *
 * Synchronous behaviour that the original spends frames on (MemCardOpen of a missing file blocks for about two accept cycles) is
 * not reproduced: the native call returns at once and the frame timeline is shorter by that amount (recorded as a frame shift). */
#include "jojo_guest.h"
#include "jojo_core.h"
#include "save.h"
#include "mod_plugins.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern void jojo_lle_func_80044FC0(CPUState *cpu);
extern void jojo_lle_func_80044FF0(CPUState *cpu);
extern void jojo_lle_func_80045860(CPUState *cpu);
extern void jojo_lle_func_800458D0(CPUState *cpu);
extern void jojo_lle_func_80045910(CPUState *cpu);
extern void jojo_lle_func_80045C94(CPUState *cpu);
extern void jojo_lle_func_80045FA4(CPUState *cpu);
extern void jojo_lle_func_8004612C(CPUState *cpu);
extern void jojo_lle_func_80046170(CPUState *cpu);
extern void jojo_lle_func_800467B0(CPUState *cpu);
extern void jojo_lle_func_80046C64(CPUState *cpu);
extern void jojo_lle_func_80046D94(CPUState *cpu);
extern void jojo_lle_func_800472FC(CPUState *cpu);
extern void jojo_lle_func_80050920(CPUState *cpu);
extern void jojo_lle_func_80050990(CPUState *cpu);
extern void jojo_lle_func_800510BC(CPUState *cpu);
extern void jojo_lle_func_8005149C(CPUState *cpu);
extern void jojo_lle_func_80052C94(CPUState *cpu);
extern void jojo_lle_func_80052DC4(CPUState *cpu);
extern void jojo_lle_func_80053214(CPUState *cpu);

extern uint8_t psx_read_byte(uint32_t addr);
extern uint32_t psx_read_word(uint32_t addr);
extern void psx_write_byte(uint32_t addr, uint8_t value);
extern void psx_write_word(uint32_t addr, uint32_t value);
extern uint64_t jojo_frame_now(void);

#define JOJO_SAVE_NAME_CAP 32
#define JOJO_SAVE_BLOCK 8192u
#define JOJO_SAVE_MAX_CHANNEL 2u

/* libmcrd operation ids (value written to the libmcrd state word, passed to the callback). */
#define OP_EXIST  1u
#define OP_ACCEPT 2u
#define OP_WRITE  4u
#define OP_READ   5u

/* Measured issue-to-callback latencies in frames. ACCEPT is measured (80 frames in two reference sessions). The others are not yet
 * measured and are GUESS placeholders; they are replaced when a REFERENCE save run gives numbers. */
#define LAT_ACCEPT 80u
#define LAT_EXIST  80u   /* GUESS */
#define LAT_READ   80u   /* GUESS */
#define LAT_WRITE  140u  /* measured: reference write issued at 26253, callback at 26393 (phase1-145) */

/* Return address used for guest callback calls (same convention as jojo_cd.c). */
#define CALLBACK_RETURN 0x80049370u

typedef struct {
    int active;
    uint32_t op;
    uint32_t code;          /* result code the callback receives */
    uint64_t due_frame;
    PlatSaveFile file;
    uint32_t guest_buf;     /* read: destination */
    uint32_t offset;
    uint32_t size;
    unsigned char *payload; /* write: snapshot taken at issue */
} PendingOp;

static int g_started;
static int g_first_accept;          /* the first accept after MemCardStart reports result 3 (reference), later ones 0 */
static uint32_t g_callback;         /* Capcom callback set by MemCardCallback */
static PendingOp g_op;
static int g_open;                  /* a save file is open through MemCardOpen */
static PlatSaveFile g_open_file;
static uint32_t g_chan_bits;        /* libmcrd per-channel bits (set by CreateFile) */
static int g_save_ready;

#define LLE_CALL(addr, fn) do { \
    uint64_t start_ = jojo_cycles_now(); \
    fn(cpu); \
    jojo_note_cycles((addr), jojo_cycles_now() - start_); \
} while (0)

#define TRACE4(addr) jojo_trace_call((addr), a0, a1, a2, a3, cpu->gpr[2])
#define DONE(addr) do { TRACE4(addr); jojo_return(cpu); } while (0)

static int ensure_save_ready(void) {
    if (g_save_ready) return 1;
    if (plat_save_init(NULL) != 0) {
        fprintf(stderr, "jojo-saves: %s\n", plat_save_error());
        return 0;
    }
    g_save_ready = 1;
    return 1;
}

static int read_guest_name(uint32_t address, char out[JOJO_SAVE_NAME_CAP]) {
    if (!address) return 0;
    for (size_t i = 0; i < JOJO_SAVE_NAME_CAP; ++i) {
        unsigned char c = psx_read_byte(address + (uint32_t)i);
        out[i] = (char)c;
        if (c == 0) return i != 0;
    }
    out[JOJO_SAVE_NAME_CAP - 1] = '\0';
    return 0;
}

static int get_save_file(uint32_t guest_name, PlatSaveFile *file) {
    char name[JOJO_SAVE_NAME_CAP];
    return read_guest_name(guest_name, name) && plat_save_file_from_card_name(name, file);
}

static int channel_ok(uint32_t channel) {
    return channel < JOJO_SAVE_MAX_CHANNEL;
}

static void clear_pending(void) {
    free(g_op.payload);
    memset(&g_op, 0, sizeof g_op);
}

static void reset_state(void) {
    clear_pending();
    g_open = 0;
    g_chan_bits = 0;
    g_first_accept = 1;
    g_callback = 0;
}

/* Starts an asynchronous operation. Only one may be in flight, as in the original (a second request is refused). */
static int begin_op(uint32_t op, uint32_t code, uint32_t latency) {
    if (g_op.active) return 0;
    memset(&g_op, 0, sizeof g_op);
    g_op.active = 1;
    g_op.op = op;
    g_op.code = code;
    g_op.due_frame = jojo_frame_now() + latency;
    return 1;
}

/* Calls the Capcom callback. The card callback 0x80015598 only stores (op, code) in the Capcom job struct at 0x80077E98 (op at +8,
 * code at +12, a code of 0 is stored as 255), so it is applied natively. A guest call from inside the vblank tick deadlocks the
 * emulator: the nested call takes a vblank interrupt whose handler waits on a timer (phase1-172 hang.sample.txt). Other callbacks still
 * run as guest code. */
#define CAPCOM_CARD_CALLBACK 0x80015598u
static void deliver_callback(CPUState *cpu, uint32_t op, uint32_t code) {
    if (!g_callback) return;
    if (g_callback == CAPCOM_CARD_CALLBACK) {
        psx_write_word(0x80077EA0u, op);
        psx_write_word(0x80077EA4u, code ? code : 255u);
        jojo_trace_call(CAPCOM_CARD_CALLBACK, op, code, 0u, 0u, 0u);   /* same line format as the reference callback entry */
        return;
    }
    (void)psx_mod_call_guest(cpu, g_callback, CALLBACK_RETURN, op, code, 0u, 0u);
}

/* Runs the operation's file work and calls the Capcom callback with (op, code). Called from jojo_saves_frame_tick or flushed by
 * MemCardStop. The file work happens at completion, as in the original card state machine. */
static void complete_op(CPUState *cpu) {
    PendingOp op = g_op;
    uint32_t code = op.code;
    g_op.active = 0;
    g_op.payload = NULL;
    if (op.op == OP_READ) {
        unsigned char *buf = (unsigned char *)malloc(op.size ? op.size : 1u);
        if (!buf || plat_save_read(op.file, op.offset, buf, op.size) != 0) {
            fprintf(stderr, "jojo-saves: read failed: %s\n", plat_save_error());
            code = 1u;
        } else {
            for (uint32_t i = 0; i < op.size; ++i) psx_write_byte(op.guest_buf + i, buf[i]);
        }
        free(buf);
    } else if (op.op == OP_WRITE) {
        if (plat_save_write(op.file, op.offset, op.payload, op.size) != 0) {
            fprintf(stderr, "jojo-saves: write failed: %s\n", plat_save_error());
            code = 1u;
        }
    }
    free(op.payload);
    deliver_callback(cpu, op.op, code);
}

/* ---------------------------------------------------------------- libmcrd: init, start, stop */
/* The reference MemCardInit (bu_init) returns only after about 148 frames (issued at frame 192, back in the caller at frame 340,
 * measured in phase1-165 and phase1-166). The native call cannot wait, so it charges the same guest time instead. */
#define INIT_CHARGE_FRAMES 148u
#define CYCLES_PER_FRAME 564480u   /* 33868800 Hz / 60 */
void func_80044FC0(CPUState *cpu) {   /* MemCardInit */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80044FC0u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x80044FC0u, jojo_lle_func_80044FC0);
        TRACE4(0x80044FC0u);
        return;
    }
    jojo_cycles_charge(INIT_CHARGE_FRAMES * CYCLES_PER_FRAME);
    cpu->gpr[2] = 0;
    DONE(0x80044FC0u);
}

void func_80044FF0(CPUState *cpu) {   /* MemCardEnd */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80044FF0u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x80044FF0u, jojo_lle_func_80044FF0);
        TRACE4(0x80044FF0u);
        return;
    }
    cpu->gpr[2] = 0;
    DONE(0x80044FF0u);
}

void func_80045860(CPUState *cpu) {   /* MemCardStart */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80045860u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x80045860u, jojo_lle_func_80045860);
        TRACE4(0x80045860u);
        return;
    }
    reset_state();
    g_started = 1;
    cpu->gpr[2] = ensure_save_ready() ? 0u : 1u;
    DONE(0x80045860u);
}

void func_800458D0(CPUState *cpu) {   /* MemCardStop: the original waits for a pending operation; this one completes it now */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x800458D0u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x800458D0u, jojo_lle_func_800458D0);
        TRACE4(0x800458D0u);
        return;
    }
    if (g_op.active) complete_op(cpu);
    g_started = 0;
    g_open = 0;
    cpu->gpr[2] = 0;
    DONE(0x800458D0u);
}

/* ---------------------------------------------------------------- libmcrd: asynchronous requests */
void func_80045910(CPUState *cpu) {   /* MemCardExist(chan): card-level check; code 3 when the channel bit is clear, else 0 (GUESS) */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80045910u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x80045910u, jojo_lle_func_80045910);
        TRACE4(0x80045910u);
        return;
    }
    int ok = channel_ok(a0) && g_started && begin_op(OP_EXIST, (g_chan_bits >> a0) & 1u ? 0u : 3u, LAT_EXIST);
    cpu->gpr[2] = ok ? 1u : 0u;
    DONE(0x80045910u);
}

void func_80045C94(CPUState *cpu) {   /* MemCardAccept(chan): the first accept after MemCardStart reports 3, later ones 0 (measured) */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80045c94u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x80045c94u, jojo_lle_func_80045C94);
        TRACE4(0x80045c94u);
        return;
    }
    uint32_t code = g_first_accept ? 3u : 0u;
    int ok = channel_ok(a0) && g_started && begin_op(OP_ACCEPT, code, LAT_ACCEPT);
    if (ok) g_first_accept = 0;
    cpu->gpr[2] = ok ? 1u : 0u;
    DONE(0x80045c94u);
}

void func_80046170(CPUState *cpu) {   /* MemCardReadData(buf, offset, size) on the open file; completion copies into buf */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80046170u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x80046170u, jojo_lle_func_80046170);
        TRACE4(0x80046170u);
        return;
    }
    int ok = g_started && g_open && !g_op.active && (a2 & 0x7Fu) == 0 && (a1 & 0x7Fu) == 0 && a2 <= PLAT_SAVE_MAX_BYTES &&
             a1 <= PLAT_SAVE_MAX_BYTES && a1 + a2 <= PLAT_SAVE_MAX_BYTES && ensure_save_ready();
    if (ok && begin_op(OP_READ, 0u, LAT_READ)) {
        g_op.file = g_open_file;
        g_op.guest_buf = a0;
        g_op.offset = a1;
        g_op.size = a2;
        cpu->gpr[2] = 1u;
    } else {
        if (!g_open) fprintf(stderr, "jojo-saves: read with no open file\n");
        cpu->gpr[2] = 0u;
    }
    DONE(0x80046170u);
}

/* JOJO_SAVE_ISSUE_DUMP=<prefix> (diagnostics, both builds): every MemCardWriteFile call writes the guest buffer as it is at the
 * call to <prefix>.<n>.bin and one line (frame, name, offset, size) to stderr. Compared with the card image of the reference,
 * this shows whether the original stores the buffer as it was at issue (the native contract) or as it is later. */
static void save_issue_dump(uint32_t name, uint32_t buf, uint32_t offset, uint32_t size) {
    static int n;
    const char *p = getenv("JOJO_SAVE_ISSUE_DUMP");
    char path[512], fname[24];
    FILE *f;
    uint32_t i;
    if (!p || !*p || size > PLAT_SAVE_MAX_BYTES) return;
    for (i = 0; i + 1 < sizeof fname && psx_read_byte(name + i); ++i) fname[i] = (char)psx_read_byte(name + i);
    fname[i] = 0;
    snprintf(path, sizeof path, "%s.%d.bin", p, n);
    fprintf(stderr, "jojo-saves: issue dump %d frame %llu name %s offset 0x%X size 0x%X buf %08X\n", n,
            (unsigned long long)jojo_frame_now(), fname, offset, size, buf);
    n++;
    if (!(f = fopen(path, "wb"))) return;
    for (i = 0; i < size; ++i) fputc(psx_read_byte(buf + i), f);
    fclose(f);
}

void func_800467B0(CPUState *cpu) {   /* MemCardWriteFile(chan, name, buf, offset, size): payload is snapshotted at issue */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x800467b0u, cpu->gpr[31]);
    save_issue_dump(a1, a2, a3, psx_read_word(cpu->gpr[29] + 16u));
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x800467b0u, jojo_lle_func_800467B0);
        TRACE4(0x800467b0u);
        return;
    }
    uint32_t size = psx_read_word(cpu->gpr[29] + 16u);
    PlatSaveFile file;
    int ok = channel_ok(a0) && g_started && !g_open && get_save_file(a1, &file) && (size & 0x7Fu) == 0 && (a3 & 0x7Fu) == 0 &&
             size <= PLAT_SAVE_MAX_BYTES && a3 <= PLAT_SAVE_MAX_BYTES && a3 + size <= PLAT_SAVE_MAX_BYTES && ensure_save_ready() &&
             !g_op.active;
    if (ok) {
        unsigned char *payload = (unsigned char *)malloc(size ? size : 1u);
        ok = payload != NULL;
        if (ok) {
            for (uint32_t i = 0; i < size; ++i) payload[i] = psx_read_byte(a2 + i);
            ok = begin_op(OP_WRITE, 0u, LAT_WRITE);
            if (ok) {
                g_op.file = file;
                g_op.offset = a3;
                g_op.size = size;
                g_op.payload = payload;
            } else {
                free(payload);
            }
        }
    }
    if (!ok) fprintf(stderr, "jojo-saves: write refused (chan %u, offset 0x%X, size 0x%X)\n", a0, a3, size);
    cpu->gpr[2] = ok ? 1u : 0u;
    DONE(0x800467b0u);
}

/* ---------------------------------------------------------------- libmcrd: open, close, callback, create, format */
void func_80045FA4(CPUState *cpu) {   /* MemCardOpen(chan, name, mode): returns at once (the original blocks through two accept cycles when the file is missing) */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80045fa4u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x80045fa4u, jojo_lle_func_80045FA4);
        TRACE4(0x80045fa4u);
        return;
    }
    PlatSaveFile file;
    uint32_t result;
    if (g_open) {
        fprintf(stderr, "jojo-saves: MemCardOpen while a file is open\n");
        result = 0xFFFFFFFFu;
    } else if (!channel_ok(a0) || !g_started || !get_save_file(a1, &file) || !ensure_save_ready()) {
        result = 0xFFFFFFFFu;
    } else if (plat_save_exists(file) == 1) {
        g_open = 1;
        g_open_file = file;
        result = 0u;
    } else {
        result = 5u;   /* no file: the value the original returns after its accept cycles */
    }
    (void)a2; (void)a3;
    cpu->gpr[2] = result;
    DONE(0x80045fa4u);
}

void func_8004612C(CPUState *cpu) {   /* MemCardClose: closes the open file; -1 when a file was closed */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x8004612cu, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x8004612cu, jojo_lle_func_8004612C);
        TRACE4(0x8004612cu);
        return;
    }
    if (g_open) {
        g_open = 0;
        cpu->gpr[2] = 0xFFFFFFFFu;
    } else {
        cpu->gpr[2] = 0u;
    }
    DONE(0x8004612cu);
}

void func_80046C64(CPUState *cpu) {   /* MemCardCallback(fn): returns the previous callback */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80046c64u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x80046c64u, jojo_lle_func_80046C64);
        TRACE4(0x80046c64u);
        return;
    }
    cpu->gpr[2] = g_callback;
    g_callback = a0;
    DONE(0x80046c64u);
}

void func_80046D94(CPUState *cpu) {   /* MemCardCreateFile(chan, name, blocks): synchronous; the file is created empty and filled by the write */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80046d94u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x80046d94u, jojo_lle_func_80046D94);
        TRACE4(0x80046d94u);
        return;
    }
    PlatSaveFile file;
    uint32_t result;
    if (g_op.active) {
        result = 0xFFFFFFFFu;               /* "system busy" */
    } else if (!channel_ok(a0) || !g_started || !get_save_file(a1, &file) || !ensure_save_ready() || a2 == 0u) {
        result = 1u;
    } else if (plat_save_write(file, 0u, NULL, 0u) != 0) {
        fprintf(stderr, "jojo-saves: create failed: %s\n", plat_save_error());
        result = 1u;
    } else {
        g_chan_bits |= 1u << a0;
        result = 0u;
    }
    cpu->gpr[2] = result;
    DONE(0x80046d94u);
}

void func_800472FC(CPUState *cpu) {   /* card format for a channel (GUESS: removes both save files; the game has no other card data) */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x800472fcu, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x800472fcu, jojo_lle_func_800472FC);
        TRACE4(0x800472fcu);
        return;
    }
    uint32_t result;
    if (g_op.active || g_open || !channel_ok(a0) || !g_started || !ensure_save_ready()) {
        result = 0xFFFFFFFFu;
    } else {
        int ok = 1;
        for (int i = 0; i < PLAT_SAVE_FILE_COUNT; ++i)
            if (plat_save_exists((PlatSaveFile)i) == 1 && plat_save_remove((PlatSaveFile)i) != 0) ok = 0;
        result = ok ? 0u : 1u;
    }
    cpu->gpr[2] = result;
    DONE(0x800472fcu);
}

/* ---------------------------------------------------------------- libmcx: no PocketStation present */
/* Capcom reads these results as "no PocketStation" (see 0x80014F28 and 0x80014D44): McxGetApl writes apl 0 and the McxSync result is
 * 0 (completed, not an error or retry). Show/hide transfer and the start/stop of the command layer do nothing. */
void func_80050920(CPUState *cpu) {   /* McxStartCom */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80050920u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x80050920u, jojo_lle_func_80050920);
        TRACE4(0x80050920u);
        return;
    }
    cpu->gpr[2] = 0;
    DONE(0x80050920u);
}

void func_80050990(CPUState *cpu) {   /* McxStopCom */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80050990u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x80050990u, jojo_lle_func_80050990);
        TRACE4(0x80050990u);
        return;
    }
    cpu->gpr[2] = 0;
    DONE(0x80050990u);
}

/* The PocketStation query (libmcx command 19, 0x80053214) sent to a slot that holds an ordinary memory card. The original McxSync
 * (0x800510BC) then ends in an error state and reports result 2 (GUESS between 1 and 2: both make the download job 0x80014D44 stop
 * with message 4, 2 after one retry). Result 0 means a PocketStation answered and would let the job write JOJOX. */
static int g_mcx_query_pending;

void func_800510BC(CPUState *cpu) {   /* McxSync(mode, cmd, result): 0 when no command is in flight; 2 after a PocketStation query */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x800510bcu, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x800510bcu, jojo_lle_func_800510BC);
        TRACE4(0x800510bcu);
        return;
    }
    if (g_mcx_query_pending) {
        g_mcx_query_pending = 0;
        if (a1) psx_write_word(a1, 19u);
        if (a2) psx_write_word(a2, 2u);
        cpu->gpr[2] = 1;
        DONE(0x800510bcu);
    }
    if (a1) psx_write_word(a1, 0u);
    if (a2) psx_write_word(a2, 0u);
    cpu->gpr[2] = (uint32_t)-1;
    DONE(0x800510bcu);
}

void func_8005149C(CPUState *cpu) {   /* McxGetApl(port, int *apl): apl 0 = no PocketStation */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x8005149cu, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x8005149cu, jojo_lle_func_8005149C);
        TRACE4(0x8005149cu);
        return;
    }
    if (a1) psx_write_word(a1, 0u);
    cpu->gpr[2] = 0;
    DONE(0x8005149cu);
}

void func_80052C94(CPUState *cpu) {   /* McxShowTrans(port, dir, timeout): no device, nothing to show */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80052c94u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x80052c94u, jojo_lle_func_80052C94);
        TRACE4(0x80052c94u);
        return;
    }
    cpu->gpr[2] = 0;
    DONE(0x80052c94u);
}

void func_80052DC4(CPUState *cpu) {   /* McxHideTrans(port) */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80052dc4u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x80052dc4u, jojo_lle_func_80052DC4);
        TRACE4(0x80052dc4u);
        return;
    }
    cpu->gpr[2] = 0;
    DONE(0x80052dc4u);
}

void func_80053214(CPUState *cpu) {   /* libmcx command 19, the PocketStation query of the download job: an ordinary card answers */
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80053214u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_SAVES)) {
        LLE_CALL(0x80053214u, jojo_lle_func_80053214);
        TRACE4(0x80053214u);
        return;
    }
    g_mcx_query_pending = 1;
    cpu->gpr[2] = 1;
    DONE(0x80053214u);
}

/* ---------------------------------------------------------------- per-vblank tick */
/* Runs once per vblank after the guest quantum (main.cpp, next to jojo_cd_frame_tick). Delivers a due completion, then applies the
 * harness pokes. Without JOJO_SAVES=1 there is no pending card operation and only the pokes can act. */


/* ---------------------------------------------------------------- harness pokes
 * JOJO_POKES=<file> with lines "<frame> <hex address> <hex byte>" writes one guest byte after the quantum of that frame. Both sides of
 * a comparison get the same file, so the write lands on the same frame in both runs. Development only; no-op without the variable. */
#define JOJO_POKE_CAP 256
typedef struct { uint64_t frame; uint32_t addr; uint8_t value; } JojoPoke;
static JojoPoke g_pokes[JOJO_POKE_CAP];
static int g_poke_count = -1;
static int g_poke_next;

static void load_pokes(void) {
    g_poke_count = 0;
    const char *path = getenv("JOJO_POKES");
    if (!path || !*path) return;
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "jojo-saves: cannot open JOJO_POKES %s\n", path); return; }
    char line[256];
    while (fgets(line, sizeof line, f)) {
        unsigned long long frame; unsigned addr, value;
        if (sscanf(line, "%llu %x %x", &frame, &addr, &value) != 3) continue;
        if (g_poke_count == JOJO_POKE_CAP) { fprintf(stderr, "jojo-saves: too many pokes\n"); break; }
        g_pokes[g_poke_count].frame = frame;
        g_pokes[g_poke_count].addr = addr;
        g_pokes[g_poke_count].value = (uint8_t)value;
        g_poke_count++;
    }
    fclose(f);
    fprintf(stderr, "jojo-saves: %d harness pokes from %s\n", g_poke_count, path);
}

void jojo_saves_frame_tick(CPUState *cpu, uint64_t frame) {
    if (g_op.active && jojo_piece_on(JOJO_PIECE_SAVES) && frame >= g_op.due_frame) complete_op(cpu);
    if (g_poke_count < 0) load_pokes();
    while (g_poke_next < g_poke_count && g_pokes[g_poke_next].frame <= frame) {
        if (g_pokes[g_poke_next].frame == frame)
            psx_write_byte(g_pokes[g_poke_next].addr, g_pokes[g_poke_next].value);
        g_poke_next++;
    }
}
