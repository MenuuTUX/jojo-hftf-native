/* Phase 1 hybrid adapter for the Capcom loader's libcd calls.
 *
 * The platform stream reads extracted files and schedules native events. This
 * file alone owns guest addresses, CPUState and callback dispatch. It never
 * calls the original libcd body while JOJO_CD is enabled.
 *
 * XA requests are resolved against extracted_xa/index.tsv and exposed through
 * jojo_cd_xa_poll()/jojo_cd_xa_read(). Their audio-engine consumer is still an
 * explicit dependency on the audio worker; this adapter does not claim to
 * produce sound on its own.
 */
#include "jojo_guest.h"
#include "jojo_core.h"
#include "cd_stream.h"
#include "audio_xa.h"
#include "mod_plugins.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef JOJO_EXTRACTED_DIR_DEFAULT
#define JOJO_EXTRACTED_DIR_DEFAULT "extracted"
#endif
#ifndef JOJO_EXTRACTED_XA_DIR_DEFAULT
#define JOJO_EXTRACTED_XA_DIR_DEFAULT "extracted_xa"
#endif

#define CD_SECTOR_GUEST_BUFFER 0x80111000u
#define CD_CALLBACK_SLOTS 32u
#define CD_XA_QUEUE_CAPACITY 16u

enum {
    CD_TOKEN_NONE = 0,
    CD_TOKEN_READ = 1,
    CD_TOKEN_SEEK = 2,
    CD_TOKEN_CONTROL = 3,
    CD_TOKEN_ALARM = 4,
    CD_TOKEN_INDIRECT = 5
};

typedef struct {
    uint32_t id;
    uint32_t address;
    int persistent;
    int used;
} CallbackBinding;

static JojoCdStream *g_cd;
static uint32_t g_ready_token;     /* token of the running ready system (0 when none) */
static CallbackBinding g_callbacks[CD_CALLBACK_SLOTS];
static uint32_t g_next_callback_id = 1;
static uint8_t g_sector[JOJO_CD_SECTOR_BYTES];
static int g_sector_valid;
static uint32_t g_sync_callback;
static uint32_t g_ready_callback;
static uint32_t g_read_callback;
static uint32_t g_seek_callback;
static uint32_t g_alarm_callback;
static uint8_t g_status;
static uint8_t g_mode;
static uint8_t g_filter_file;
static uint8_t g_filter_channel;
static uint8_t g_debug;
static uint32_t g_read_token;
static uint64_t g_read_start_frame;
static uint64_t g_read_sector_count;
static uint64_t g_control_count;
static uint64_t g_event_count;
static uint64_t g_latency_sum;
static uint64_t g_latency_max;
static uint64_t g_latency_samples;
static JojoCdEvent g_xa_queue[CD_XA_QUEUE_CAPACITY];
static size_t g_xa_head;
static size_t g_xa_count;
static int g_reported_audio_dependency;

extern void jojo_lle_func_80049370(CPUState *cpu);
extern void jojo_lle_func_800494B4(CPUState *cpu);
extern void jojo_lle_func_800495E0(CPUState *cpu);
extern void jojo_lle_func_80049824(CPUState *cpu);
extern void jojo_lle_func_80049BC8(CPUState *cpu);
extern void jojo_lle_func_80049E4C(CPUState *cpu);
extern void jojo_lle_func_80049EDC(CPUState *cpu);
extern void jojo_lle_func_80049F28(CPUState *cpu);
extern void jojo_lle_func_80049F38(CPUState *cpu);
extern void jojo_lle_func_8004CB00(CPUState *cpu);
extern void jojo_lle_func_8004CB20(CPUState *cpu);
extern void jojo_lle_func_8004CB80(CPUState *cpu);
extern void jojo_lle_func_8004CC84(CPUState *cpu);
extern void jojo_lle_func_8004D280(CPUState *cpu);
extern void jojo_lle_func_8004D304(CPUState *cpu);
extern void jojo_lle_func_8004D368(CPUState *cpu);
extern void jojo_lle_func_8004D820(CPUState *cpu);
extern void jojo_lle_func_8004D834(CPUState *cpu);
extern void jojo_lle_func_8004D92C(CPUState *cpu);
extern void jojo_lle_func_8004D9F8(CPUState *cpu);
extern void jojo_pad_func_8004D92C(CPUState *cpu);

#define TRACE(addr) jojo_trace_call((addr), a0, a1, a2, a3, cpu->gpr[2])
#define DONE(addr) do { TRACE(addr); jojo_return(cpu); } while (0)
#define LLE(addr, fn) do { \
    uint64_t start_ = jojo_cycles_now(); \
    fn(cpu); \
    jojo_note_cycles((addr), jojo_cycles_now() - start_); \
    TRACE(addr); \
    return; \
} while (0)
#define BEGIN(addr) do { \
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5]; \
    uint32_t a2 = cpu->gpr[6], a3 = cpu->gpr[7]; \
    jojo_note_call((addr), cpu->gpr[31]); \
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE((addr), jojo_lle_func_##addr); \
} while (0)

static void cd_fatal(const char *what) {
    fprintf(stderr, "jojo-cd: unsupported or failed native CD operation: %s\n",
            what ? what : "unknown");
    abort();
}

static int callback_supported(uint32_t address) {
    /* Any function of the exe text (the Capcom loader and its sync callbacks, e.g. 0x80017C14 besides 0x80017CC8). */
    return address >= 0x80010000u && address < 0x80099800u;
}

static void derive_xa_root(const char *data_root, char out[1024]) {
    const char *env = getenv("JOJO_EXTRACTED_XA_DIR");
    size_t n;
    if (env && *env) {
        snprintf(out, 1024, "%s", env);
        return;
    }
    n = strlen(data_root);
    if (n >= 10u && strcmp(data_root + n - 10u, "/extracted") == 0 && n + 4u < 1024u) {   /* "/extracted" is 10 bytes */
        memcpy(out, data_root, n - 10u);
        strcpy(out + n - 10u, "/extracted_xa");
        return;
    }
    snprintf(out, 1024, "%s", JOJO_EXTRACTED_XA_DIR_DEFAULT);
}

static void jojo_cd_timing_report(void) {
    if (g_latency_samples) {
        fprintf(stderr,
                "jojo-cd timing: callbacks=%llu avg_delay=%.2f frames max_delay=%llu frames read_sectors=%llu controls=%llu\n",
                (unsigned long long)g_latency_samples,
                (double)g_latency_sum / (double)g_latency_samples,
                (unsigned long long)g_latency_max,
                (unsigned long long)g_read_sector_count,
                (unsigned long long)g_control_count);
    } else if (g_cd) {
        fprintf(stderr, "jojo-cd timing: no native callbacks were delivered\n");
    }
    if (g_cd) {
        jojo_cd_stream_close(g_cd);
        g_cd = NULL;
    }
}

static JojoCdStream *ensure_cd(void) {
    const char *data_root = getenv("JOJO_EXTRACTED_DIR");
    char xa_root[1024];
    if (g_cd) return g_cd;
    if (!data_root || !*data_root) data_root = JOJO_EXTRACTED_DIR_DEFAULT;
    derive_xa_root(data_root, xa_root);
    g_cd = jojo_cd_stream_open(data_root, xa_root);
    if (!g_cd) {
        fprintf(stderr, "jojo-cd: cannot open extracted data at %s\n", data_root);
        abort();
    }
    (void)atexit(jojo_cd_timing_report);
    fprintf(stderr, "jojo-cd: native file stream enabled (%s; XA index %s)\n",
            data_root, xa_root);
    return g_cd;
}

static uint32_t bind_callback(uint32_t address, int persistent) {
    uint32_t id;
    if (!address) return 0;
    if (!callback_supported(address)) {
        char message[96];
        snprintf(message, sizeof message, "callback address 0x%08X", address);
        cd_fatal(message);
    }
    for (size_t i = 0; i < CD_CALLBACK_SLOTS; ++i) {
        if (!g_callbacks[i].used) {
            id = g_next_callback_id++;
            if (!id) id = g_next_callback_id++;
            g_callbacks[i].id = id;
            g_callbacks[i].address = address;
            g_callbacks[i].persistent = persistent;
            g_callbacks[i].used = 1;
            return id;
        }
    }
    cd_fatal("callback binding table exhausted");
    return 0;
}

static uint32_t callback_address(uint32_t id, int release) {
    if (!id) return 0;
    for (size_t i = 0; i < CD_CALLBACK_SLOTS; ++i) {
        CallbackBinding *binding = &g_callbacks[i];
        if (binding->used && binding->id == id) {
            uint32_t address = binding->address;
            if (release == 2 || (release && !binding->persistent)) binding->used = 0;   /* 2: retire a persistent ReadN binding */
            return address;
        }
    }
    cd_fatal("stale callback token");
    return 0;
}

static uint32_t call_guest(CPUState *cpu, uint32_t address,
                           uint32_t a0, uint32_t a1,
                           uint32_t a2, uint32_t a3) {
    uint32_t return_address;
    if (!address) return 0;
    if (!callback_supported(address)) {
        char message[96];
        snprintf(message, sizeof message, "guest callback 0x%08X", address);
        cd_fatal(message);
    }
    return_address = cpu->pc ? cpu->pc : 0x80049370u;
    /* The original runs these callbacks from the CD interrupt handler, where SR.IEc is 0, so no other interrupt can be taken
     * inside them. The nested call keeps IEc clear for the same reason: with it set, a pending vblank was dispatched inside the
     * callback, and the original pad driver's handler (pad piece off) spun there forever. */
    {
        uint32_t sr = cpu->cop0[12], result;
        cpu->cop0[12] = sr & ~1u;
        result = psx_mod_call_guest(cpu, address, return_address, a0, a1, a2, a3);
        cpu->cop0[12] = (cpu->cop0[12] & ~1u) | (sr & 1u);
        return result;
    }
}

static uint8_t guest_byte(CPUState *cpu, uint32_t address) {
    if (!address || !cpu->read_byte) cd_fatal("invalid guest byte read");
    return cpu->read_byte(address);
}

static void guest_write(CPUState *cpu, uint32_t address,
                        const uint8_t *src, size_t bytes) {
    if (!address || !cpu->write_byte) cd_fatal("invalid guest buffer write");
    for (size_t i = 0; i < bytes; ++i)
        cpu->write_byte(address + (uint32_t)i, src[i]);
}

static uint32_t new_token(uint32_t address, int persistent) {
    return bind_callback(address, persistent);
}

/* DsCommand: queues one CdControl and returns its command id (never 0 when queued; the loaders poll until nonzero). */
static uint32_t g_cmd_id;

static uint32_t queue_control(uint8_t command) {
    uint32_t token = new_token(g_sync_callback, 0);
    uint32_t id = ++g_cmd_id;
    if (!jojo_cd_stream_submit(ensure_cd(), JOJO_CD_OP_CONTROL, command,
                               id, 0, token)) {
        callback_address(token, 1);
        return 0;
    }
    ++g_control_count;
    return id;
}

static uint32_t bcd_to_binary(uint8_t value) {
    return (uint32_t)((value >> 4) * 10u + (value & 0x0fu));
}

static uint8_t binary_to_bcd(uint32_t value) {
    return (uint8_t)(((value / 10u) << 4) | (value % 10u));
}

static uint32_t pos_to_lba(CPUState *cpu, uint32_t address) {
    uint32_t minute = bcd_to_binary(guest_byte(cpu, address));
    uint32_t second = bcd_to_binary(guest_byte(cpu, address + 1u));
    uint32_t frame = bcd_to_binary(guest_byte(cpu, address + 2u));
    return (minute * 60u + second) * 75u + frame - 150u;
}

static void lba_to_pos(CPUState *cpu, uint32_t lba, uint32_t address) {
    uint32_t absolute = lba + 150u;
    uint8_t pos[4];
    pos[0] = binary_to_bcd(absolute / (60u * 75u));
    pos[1] = binary_to_bcd((absolute / 75u) % 60u);
    pos[2] = binary_to_bcd(absolute % 75u);
    pos[3] = 0;
    guest_write(cpu, address, pos, sizeof pos);
}

/* ---- XA music (native) ----------------------------------------------------
 * A ReadN starts a stream at its LBA (XA playback by the controller is the audio sectors the read passes).
 * The rows of extracted_xa/index.tsv are those sectors. With Setmode bit 0x40 on, the next rows are decoded
 * (platform/audio_xa) and pushed to the CD bus of the native mixer while it holds less than one 37.8 kHz
 * sector (2352 frames) ahead, so the stream is paced by the mixer clock (the reference paces it by the
 * controller, one audio sector per 53 ms on a channel at 37.8 kHz). Setmode bit 0x08 selects the file and
 * channel of Setfilter. Pause stops the stream; a new ReadN resets the decoder. Nothing is sent to the
 * emulated SPU or CD input: the native path ends in the native mixer. */
static int      g_xa_play;
static uint32_t g_xa_cursor_lba;
static int g_xa_trace_on = -1;   /* JOJO_CD_XATRACE=1: one line per delivered XA sector */
static uint32_t g_xa_pos4;
static uint32_t g_xa_start4;        /* drive position at ReadS, in quarter sectors */
static uint64_t g_xa_start_frame;   /* frame of the ReadS: the drive clock is a function of the frame, so a missed tick loses nothing */
static int      g_xa_defer;         /* the first pump after ReadS waits one tick: the job's DsMix (same frame) must run first */          /* drive position while streaming XA, in quarter sectors (drive clock, see xa_pump) */
static XaDecoder g_xa_dec;
static const uint8_t k_cd_matrix_unity[2][2] = { { 0x80, 0x00 }, { 0x00, 0x80 } };
/* CD mixer matrix as DsMix sets it (see func_8004CB00): m[source][dest], 0x80 = unity. Starts at unity, like the power-on ATV registers. */
static uint8_t g_cd_matrix[2][2] = { { 0x80, 0x00 }, { 0x00, 0x80 } };

extern size_t jojo_audio_cd_pending(void);
extern void   jojo_audio_cd_push(const int16_t *stereo, size_t frames);

static void xa_stop(void) {
    g_xa_play = 0;
    xa_decoder_reset(&g_xa_dec);
}

static int g_xa_logged_start, g_xa_logged_mode, g_xa_logged_end;

/* XA job position: the SeekL target (job +8) is where ReadS starts; GetlocP reports the last XA sector decoded. */
static uint32_t g_seek_lba;
static uint32_t g_xa_last_lba;

static void xa_start(uint32_t lba) {
    xa_decoder_reset(&g_xa_dec);
    g_xa_cursor_lba = lba;
    g_xa_last_lba = lba;
    g_xa_pos4 = lba * 4u;               /* the drive is at the SeekL target when ReadS starts */
    g_xa_start4 = g_xa_pos4;
    g_xa_start_frame = jojo_frame_now();
    g_xa_play = 1;
    g_xa_defer = 1;
    int loaded = jojo_cd_stream_xa_load(ensure_cd());
    if (!g_xa_logged_start && getenv("JOJO_CD_XALOG")) {
        g_xa_logged_start = 1;
        fprintf(stderr, "jojo-cd: XA stream at lba %u (mode %02X filter %u/%u index %d rows %zu) %s\n", (unsigned)lba,
                (unsigned)g_mode, (unsigned)g_filter_file, (unsigned)g_filter_channel, loaded,
                jojo_cd_stream_xa_rows(g_cd), jojo_cd_stream_error(g_cd));
    }
}

static void xa_pump(void) {
    static int16_t pcm[XA_MAX_OUT_FRAMES * 2];
    if (!g_xa_play || !jojo_piece_on(JOJO_PIECE_AUDIO)) return;
    if (g_xa_defer) { g_xa_defer = 0; return; }   /* first sector one tick after ReadS, as the reference (see g_xa_defer) */
    if (!(g_mode & 0x40u)) {                       /* XA-ADPCM not enabled in Setmode */
        if (!g_xa_logged_mode && getenv("JOJO_CD_XALOG")) {
            g_xa_logged_mode = 1;
            fprintf(stderr, "jojo-cd: XA stream waiting: Setmode %02X has no XA bit\n", (unsigned)g_mode);
        }
        return;
    }
    /* Drive clock (not mixer fill): the drive moves 150 sectors/s at double speed (Setmode 0x80, so 2.5 per 60 Hz frame) or
     * 75/s (1.25 per frame), reading every sector of the disc in order. An XA sector is delivered when the drive reaches its LBA;
     * sectors the filter rejects are passed by the drive and never reach the decoder. */
    /* Read-start latency: the first sector comes one single-speed sector period after ReadS (the drive spins to the target),
     * which is 2 sectors of drive travel at double speed and 1 at single speed (the runtime's initial_read_delay_cycles). */
    {
        uint32_t travel4 = (uint32_t)((jojo_frame_now() - g_xa_start_frame) * ((g_mode & 0x80u) ? 10u : 5u));
        uint32_t latency4 = (g_mode & 0x80u) ? 8u : 4u;
        g_xa_pos4 = g_xa_start4 + (travel4 > latency4 ? travel4 - latency4 : 0u);
    }
    if (g_xa_trace_on < 0) g_xa_trace_on = getenv("JOJO_CD_XATRACE") != NULL;
    for (;;) {
        JojoCdXaRow row;
        uint8_t payload[JOJO_CD_XA_PAYLOAD_BYTES];
        long row_index, n;
        row_index = jojo_cd_stream_xa_first_at_or_after(ensure_cd(), g_xa_cursor_lba);
        if (row_index < 0) {                       /* no more XA sectors after the cursor */
            if (!g_xa_logged_end && getenv("JOJO_CD_XALOG")) {
                g_xa_logged_end = 1;
                fprintf(stderr, "jojo-cd: XA rows end after lba %u\n", (unsigned)g_xa_cursor_lba);
            }
            xa_stop();
            return;
        }
        if (!jojo_cd_stream_xa_row(g_cd, (size_t)row_index, &row)) cd_fatal("XA row");
        if (row.lba >= g_xa_pos4 / 4u) return;     /* the drive has not reached this sector yet */
        g_xa_cursor_lba = row.lba + 1u;
        if ((g_mode & 0x08u) && (row.file_no != g_filter_file || row.channel != g_filter_channel))
            continue;                              /* the filter passes this sector to nobody */
        if (!jojo_cd_stream_xa_read(g_cd, (size_t)row_index, payload))
            cd_fatal(jojo_cd_stream_error(g_cd));
        n = xa_sector_pcm(&g_xa_dec, row.file_no, row.channel, row.coding, payload, pcm);
        if (n <= 0) continue;                      /* not decodable (depth or rate): skipped, as the controller does */
        xa_apply_volume(pcm, (size_t)n, g_cd_matrix);
        jojo_audio_cd_push(pcm, (size_t)n);
        if (g_xa_trace_on) fprintf(stderr, "jojo-cd xa-sector frame=%llu lba=%u pos4=%u frames=%ld matrix=%02X,%02X,%02X,%02X\n",
                                   (unsigned long long)jojo_frame_now(), (unsigned)row.lba, (unsigned)g_xa_pos4, n,
                                   g_cd_matrix[0][0], g_cd_matrix[0][1], g_cd_matrix[1][1], g_cd_matrix[1][0]);
        g_xa_last_lba = row.lba;
    }
}

static int token_live(uint32_t id) {
    for (size_t i = 0; i < CD_CALLBACK_SLOTS; ++i)
        if (g_callbacks[i].used && g_callbacks[i].id == id) return 1;
    return 0;
}

static void handle_event(CPUState *cpu, const JojoCdEvent *event) {
    /* Several sectors (each with its ready alarm) can be queued in one tick. Capcom's alarm ends the ready system after its last
     * sector, and the drive delivers nothing after that: repeating events whose token was retired are dropped, as the controller
     * would not have raised them. Other event types keep the stale-token abort. */
    if ((event->type == JOJO_CD_EVENT_ALARM || event->type == JOJO_CD_EVENT_SECTOR) && event->token && !token_live(event->token))
        return;
    uint32_t callback = callback_address(event->token, 0);
    uint64_t delay = event->frame - event->requested_frame;
    ++g_event_count;
    g_latency_sum += delay;
    if (delay > g_latency_max) g_latency_max = delay;
    ++g_latency_samples;

    switch (event->type) {
        case JOJO_CD_EVENT_CONTROL:
            if (event->command == 9u) {
                if (g_read_start_frame) {
                    fprintf(stderr,
                            "jojo-cd timing: read start=%llu pause=%llu duration=%llu frames sectors=%llu\n",
                            (unsigned long long)g_read_start_frame,
                            (unsigned long long)event->frame,
                            (unsigned long long)(event->frame - g_read_start_frame),
                            (unsigned long long)g_read_sector_count);
                }
                g_read_start_frame = 0;
                g_read_sector_count = 0;
                if (g_read_token) callback_address(g_read_token, 2);
                g_read_token = 0;
            }
            g_status = 0;
            if (event->command == 27u) xa_start(g_seek_lba);       /* ReadS: the XA job streams from the SeekL target */
            if (callback) {
                /* Capcom sync callback (status 2) with the 8-byte result block at 0x80098D40 and the command id.
                 * GetlocP (17): track, index, relative MSF, absolute MSF (BCD) of the current sector. */
                /* Reference (sampled GetlocP block vs the LBA in dword 0x8007921C): while the XA job streams, the block is track 1,
                 * index 1, with the MSF of the sector (no pregap offset); with the XA job stopped it is the drive status and zeros:
                 * 0x22 (motor on, reading) while a data ReadN streams, as the reference reports during loads, otherwise 0x02. */
                uint8_t block[8] = { 0x02, 0, 0, 0, 0, 0, 0, 0 };
                if (jojo_cd_stream_reading(g_cd)) block[0] = 0x22;
                if (event->command == 17u && g_xa_play) {
                    /* bytes 2..4: the position as the reference has it (MSF of the LBA); bytes 5..7: absolute MSF = LBA + 150
                     * (the pregap), so the original DsPosToInt(bytes 5..7) = LBA, which is what dword 0x8007921C holds. */
                    /* the sector under the head: the drive clock (g_xa_pos4) has passed every sector before it, not only the
                     * channel's own sectors (every 8th), so the last decoded XA row would trail the real report */
                    uint32_t cur = g_xa_pos4 / 4u ? g_xa_pos4 / 4u - 1u : 0u;
                    uint32_t abs = cur + 150u;
                    uint8_t rm = binary_to_bcd(cur / (60u * 75u)), rs = binary_to_bcd((cur / 75u) % 60u), rf = binary_to_bcd(cur % 75u);
                    uint8_t m = binary_to_bcd(abs / (60u * 75u)), sec = binary_to_bcd((abs / 75u) % 60u), f = binary_to_bcd(abs % 75u);
                    block[0] = 0x01; block[1] = 0x01; block[2] = rm; block[3] = rs; block[4] = rf;
                    block[5] = m; block[6] = sec; block[7] = f;
                }
                guest_write(cpu, 0x80098D40u, block, sizeof block);
                (void)call_guest(cpu, callback, 2u, 0x80098D40u, event->lba, 0);
            }
            if (g_ready_callback)
                (void)call_guest(cpu, g_ready_callback, 1u, 0, 0, 0);
            break;
        case JOJO_CD_EVENT_SECTOR:
            /* The controller holds the sector; the Capcom alarm (0x800182D4) copies it out with DsGetSector.
             * The reference never calls the ReadN callback per sector (checked in the call trace). */
            memcpy(g_sector, event->sector, sizeof g_sector);
            g_sector_valid = 1;
            ++g_read_sector_count;
            break;
        case JOJO_CD_EVENT_READY:
            /* ReadN accepted: the ReadN callback runs with status 2 (Capcom: DsStartReadySystem). */
            if (callback != 0x8001824cu)
                cd_fatal("ReadN ready did not target Capcom's ReadN callback 0x8001824C");
            (void)call_guest(cpu, callback, 2u, 0, 0, 0);
            break;
        case JOJO_CD_EVENT_SEEK:
            if (callback != 0x800181d8u)
                cd_fatal("SeekL did not target Capcom's seek callback 0x800181D8");
            g_seek_lba = event->lba;
            (void)call_guest(cpu, callback, 2u, 0, 0, 0);
            break;
        case JOJO_CD_EVENT_ALARM:
            if (callback != 0x800182d4u)
                cd_fatal("alarm did not target Capcom's callback 0x800182D4");
            (void)call_guest(cpu, callback, 1u, 0, 0, 0);
            break;
        case JOJO_CD_EVENT_XA:
            if (g_xa_count == CD_XA_QUEUE_CAPACITY)
                cd_fatal("native XA request queue exhausted before audio consumer");
            g_xa_queue[(g_xa_head + g_xa_count) % CD_XA_QUEUE_CAPACITY] = *event;
            ++g_xa_count;
            if (!g_reported_audio_dependency) {
                fprintf(stderr,
                        "jojo-cd: XA requests are queued; native audio-engine consumer is not wired yet\n");
                g_reported_audio_dependency = 1;
            }
            break;
        case JOJO_CD_EVENT_ERROR:
            cd_fatal(jojo_cd_stream_error(ensure_cd()));
            break;
        default:
            cd_fatal("unknown native CD event");
    }
    /* A repeating ready alarm keeps its token; DsEndReadySystem (run by the alarm callback itself) releases it. */
    if (event->type != JOJO_CD_EVENT_SECTOR && !(event->type == JOJO_CD_EVENT_ALARM && event->command == 1u) && callback)
        callback_address(event->token, 1);
}

/* Wire this from a runtime frame hook with the active CPUState. It drains all
 * ready CD events once per emulated frame and dispatches the loader callbacks. */
void jojo_cd_frame_tick(CPUState *cpu, uint64_t frame) {
    JojoCdEvent event;
    if (!jojo_piece_on(JOJO_PIECE_CD)) return;
    if (!cpu) cd_fatal("frame tick has no CPUState");
    jojo_cd_stream_tick(ensure_cd(), frame);
    while (jojo_cd_stream_poll(g_cd, &event)) handle_event(cpu, &event);
    xa_pump();
}

/* Audio-worker bridge: the XA index is loaded and each request is surfaced as
 * an event carrying the raw-payload filename and offset. */
int jojo_cd_xa_request(uint32_t lba, uint32_t channel) {
    return jojo_cd_stream_submit(ensure_cd(), JOJO_CD_OP_XA,
                                 (uint8_t)channel, lba, 0, 0);
}

int jojo_cd_xa_poll(JojoCdEvent *event) {
    if (!event || !g_xa_count) return 0;
    *event = g_xa_queue[g_xa_head];
    g_xa_head = (g_xa_head + 1) % CD_XA_QUEUE_CAPACITY;
    --g_xa_count;
    return 1;
}

int jojo_cd_xa_read(const JojoCdEvent *event,
                    uint8_t out[JOJO_CD_XA_PAYLOAD_BYTES]) {
    return g_cd && jojo_cd_stream_read_xa_payload(g_cd, event, out);
}

static void reset_runtime(void) {
    jojo_cd_stream_reset(ensure_cd());
    xa_stop();
    memset(g_callbacks, 0, sizeof g_callbacks);
    memset(g_sector, 0, sizeof g_sector);
    memset(g_xa_queue, 0, sizeof g_xa_queue);
    g_next_callback_id = 1;
    g_sector_valid = 0;
    g_sync_callback = 0;
    g_ready_callback = 0;
    g_read_callback = 0;
    g_seek_callback = 0;
    g_alarm_callback = 0;
    g_status = 0;
    g_mode = 0;
    g_filter_file = 0;
    g_filter_channel = 0;
    g_read_token = 0;
    g_read_start_frame = 0;
    g_read_sector_count = 0;
    g_xa_head = 0;
    g_xa_count = 0;
}

void func_80049370(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80049370u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x80049370u, jojo_lle_func_80049370);
    jojo_cd_stream_ds_init(ensure_cd(), jojo_frame_now());
    cpu->gpr[2] = 1;
    DONE(0x80049370u);
}

void func_800494B4(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x800494B4u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x800494B4u, jojo_lle_func_800494B4);
    cpu->gpr[2] = g_read_callback;
    g_read_callback = a0;
    DONE(0x800494B4u);
}

void func_800495E0(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x800495E0u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x800495E0u, jojo_lle_func_800495E0);
    if (a0 == 14u && a1) g_mode = guest_byte(cpu, a1);
    if (a0 == 13u && a1) {
        g_filter_file = guest_byte(cpu, a1);
        g_filter_channel = guest_byte(cpu, a1 + 1u);
    }
    if (a0 == 9u) xa_stop();
    /* DsCommand 21 (SeekL, the XA job's form): the MSF target at a1 becomes the position ReadS streams from. */
    if (a0 == 21u && a1) g_seek_lba = pos_to_lba(cpu, a1);
    if (getenv("JOJO_CD_XALOG")) fprintf(stderr, "jojo-cd: CdControl %u (mode now %02X)\n", (unsigned)a0, (unsigned)g_mode);
    if (a0 == 14u && a1) jojo_cd_stream_set_read_mode(ensure_cd(), g_mode);   /* the Setmode byte, for the speed-change wait */
    cpu->gpr[2] = queue_control((uint8_t)a0);
    DONE(0x800495E0u);
}

void func_80049824(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    uint32_t callback, token, lba;
    jojo_note_call(0x80049824u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x80049824u, jojo_lle_func_80049824);
    if (a2 != 6u && a2 != 21u)
        cd_fatal("CdControlB command other than ReadN/SeekL");
    callback = a3;
    if (a2 == 6u && callback != 0x8001824cu)
        cd_fatal("ReadN callback is not Capcom's 0x8001824C");
    if (a2 == 21u && callback != 0x800181d8u)
        cd_fatal("SeekL callback is not Capcom's 0x800181D8");
    lba = pos_to_lba(cpu, a1);
    token = bind_callback(callback, a2 == 6u);
    if (a2 == 6u) jojo_cd_stream_set_read_mode(ensure_cd(), (uint8_t)a0);   /* ReadN mode byte: bit 0x80 = double speed */
    if (!jojo_cd_stream_submit(ensure_cd(),
            a2 == 6u ? JOJO_CD_OP_READ : JOJO_CD_OP_SEEK,
            (uint8_t)a2, lba, 0, token)) {
        callback_address(token, 1);
        cpu->gpr[2] = 0;
    } else {
        if (a2 == 6u) {
            if (g_read_token) callback_address(g_read_token, 2);
            g_read_callback = callback;
            g_read_token = token;
            g_read_start_frame = jojo_frame_now();
            g_read_sector_count = 0;
        } else {
            g_seek_callback = callback;
        }
        cpu->gpr[2] = 1;
    }
    DONE(0x80049824u);
}

void func_80049BC8(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80049BC8u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x80049BC8u, jojo_lle_func_80049BC8);
    (void)a1; (void)a2; (void)a3;
    /* a0 = -1: queue space; otherwise the command id (the controller completes commands in order: report done) */
    if (a0 == 0xffffffffu) cpu->gpr[2] = jojo_cd_stream_pending(ensure_cd()) < JOJO_CD_QUEUE_CAPACITY;
    else cpu->gpr[2] = 1;
    DONE(0x80049BC8u);
}

void func_80049E4C(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80049E4Cu, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x80049E4Cu, jojo_lle_func_80049E4C);
    (void)a0; (void)a1; (void)a2; (void)a3;
    reset_runtime();
    cpu->gpr[2] = 1;
    DONE(0x80049E4Cu);
}

void func_80049EDC(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80049EDCu, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x80049EDCu, jojo_lle_func_80049EDC);
    (void)a0; (void)a1; (void)a2; (void)a3;
    cpu->gpr[2] = 1;
    DONE(0x80049EDCu);
}

void func_80049F28(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80049F28u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x80049F28u, jojo_lle_func_80049F28);
    (void)a0; (void)a1; (void)a2; (void)a3;
    cpu->gpr[2] = (uint32_t)jojo_cd_stream_pending(ensure_cd());
    DONE(0x80049F28u);
}

void func_80049F38(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x80049F38u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x80049F38u, jojo_lle_func_80049F38);
    (void)a0; (void)a1; (void)a2; (void)a3;
    cpu->gpr[2] = g_status;
    DONE(0x80049F38u);
}

/* DsMix(p): p[0..3] are ATV0 (CD-L -> SPU-L), ATV1 (CD-L -> SPU-R), ATV2 (CD-R -> SPU-R), ATV3 (CD-R -> SPU-L), written by the
 * original to CD index 2/3 registers 1..2 and applied by the 0x20 write to 0x1F801803. Native: the same four bytes become the
 * matrix the XA path applies (m[source][dest]). */
void func_8004CB00(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x8004CB00u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x8004CB00u, jojo_lle_func_8004CB00);
    (void)a1; (void)a2; (void)a3;
    (void)ensure_cd();
    g_cd_matrix[0][0] = guest_byte(cpu, a0);          /* ATV0: CD-L -> SPU-L */
    g_cd_matrix[0][1] = guest_byte(cpu, a0 + 1u);     /* ATV1: CD-L -> SPU-R */
    g_cd_matrix[1][1] = guest_byte(cpu, a0 + 2u);     /* ATV2: CD-R -> SPU-R */
    g_cd_matrix[1][0] = guest_byte(cpu, a0 + 3u);     /* ATV3: CD-R -> SPU-L */
    cpu->gpr[2] = 1;
    DONE(0x8004CB00u);
}

void func_8004CB20(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    size_t bytes;
    jojo_note_call(0x8004CB20u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x8004CB20u, jojo_lle_func_8004CB20);
    bytes = (size_t)a1 * 4u;
    if (bytes > JOJO_CD_SECTOR_BYTES) bytes = JOJO_CD_SECTOR_BYTES;
    if (a0 >= 0x80000000u && a0 < 0x80200000u) {          /* a RAM destination: one sector per call */
        if (g_sector_valid) {
            guest_write(cpu, a0, g_sector, bytes);
            cpu->gpr[2] = 1;
        } else {
            uint8_t zero[JOJO_CD_SECTOR_BYTES];
            memset(zero, 0, sizeof zero);
            guest_write(cpu, a0, zero, bytes);
            cpu->gpr[2] = 0;                               /* no sector held yet */
        }
    } else {
        cpu->gpr[2] = 1;                                   /* register copies are the controller's own */
    }
    (void)a2; (void)a3;
    DONE(0x8004CB20u);
}

void func_8004CB80(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x8004CB80u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x8004CB80u, jojo_lle_func_8004CB80);
    (void)a2; (void)a3;
    lba_to_pos(cpu, a0, a1);
    cpu->gpr[2] = a1;
    DONE(0x8004CB80u);
}

void func_8004CC84(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x8004CC84u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x8004CC84u, jojo_lle_func_8004CC84);
    (void)a1; (void)a2; (void)a3;
    cpu->gpr[2] = pos_to_lba(cpu, a0);
    DONE(0x8004CC84u);
}

void func_8004D280(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    uint32_t token;
    jojo_note_call(0x8004D280u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x8004D280u, jojo_lle_func_8004D280);
    if (a0 != 0x800182d4u) cd_fatal("Cd callback timer target");
    (void)a1; (void)a2; (void)a3;
    if (!g_ready_token) {
        g_alarm_callback = a0;
        token = bind_callback(a0, 1);                      /* persistent: the alarm runs once per frame */
        g_ready_token = token;
        jojo_cd_stream_ready_set(ensure_cd(), 1, token);
    }
    cpu->gpr[2] = 1;
    DONE(0x8004D280u);
}

void func_8004D304(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x8004D304u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x8004D304u, jojo_lle_func_8004D304);
    (void)a0; (void)a1; (void)a2; (void)a3;
    if (g_ready_token) {                                   /* DsEndReadySystem: stop the per-frame alarm */
        jojo_cd_stream_ready_set(ensure_cd(), 0, 0);
        callback_address(g_ready_token, 2);
        g_ready_token = 0;
    }
    cpu->gpr[2] = (uint32_t)queue_control(9u);
    DONE(0x8004D304u);
}

void func_8004D368(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x8004D368u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x8004D368u, jojo_lle_func_8004D368);
    cpu->gpr[2] = g_debug;
    g_debug = (uint8_t)a0;
    (void)a1; (void)a2; (void)a3;
    DONE(0x8004D368u);
}

void func_8004D820(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x8004D820u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x8004D820u, jojo_lle_func_8004D820);
    if (a0 && !callback_supported(a0)) cd_fatal("CdSyncCallback target");
    cpu->gpr[2] = g_sync_callback;
    g_sync_callback = a0;
    (void)a1; (void)a2; (void)a3;
    DONE(0x8004D820u);
}

void func_8004D834(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    jojo_note_call(0x8004D834u, cpu->gpr[31]);
    if (!jojo_piece_on(JOJO_PIECE_CD)) LLE(0x8004D834u, jojo_lle_func_8004D834);
    if (a0 && !callback_supported(a0)) cd_fatal("CdReadyCallback target");
    cpu->gpr[2] = g_ready_callback;
    g_ready_callback = a0;
    (void)a1; (void)a2; (void)a3;
    DONE(0x8004D834u);
}

/* 0x8004D92C (PadGetState) and 0x8004D9F8 are pad-library entry points that call through the pad driver vector at 0x80063594;
 * they sit in this family only because the surface filed them with the CD layer. With the CD piece on they run their original
 * bodies (the jalr returns through the dispatcher). A nested guest call here hung the game when the pad piece was off: the pad
 * driver's vblank handler calls 0x8004D92C from inside the exception. The pad adapter takes the per-frame call of 0x80011020
 * (saved RA 0x800110B4) when the pad piece is on. */
void func_8004D92C(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    if (cpu->gpr[31] == 0x800110B4u && jojo_piece_on(JOJO_PIECE_PAD)) {
        jojo_pad_func_8004D92C(cpu);
        return;
    }
    LLE(0x8004D92Cu, jojo_lle_func_8004D92C);
}

void func_8004D9F8(CPUState *cpu) {
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    LLE(0x8004D9F8u, jojo_lle_func_8004D9F8);
}

#undef BEGIN
