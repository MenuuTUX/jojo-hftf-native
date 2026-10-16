#include "cd_stream.h"
#include "file_layer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EVENT_CAPACITY 48u   /* a tick can complete the whole queue (8) plus its sectors and alarms */
#define XA_NAME_BYTES 64u

typedef struct {
    JojoCdOperation operation;
    uint8_t command;
    uint32_t lba;
    uint32_t delay_frames;
    uint64_t requested_frame;
    uint32_t token;
    uint8_t mode;            /* READ: the ReadN mode byte (bit 0x80 = double speed) */
} CdRequest;

typedef struct {
    char file[XA_NAME_BYTES];
    uint32_t lba;
    uint32_t channel;
    uint32_t offset;
    uint32_t file_no;
    uint32_t coding;
} XaEntry;

struct JojoCdStream {
    JojoFileLayer *files;
    char *xa_root;
    CdRequest queue[JOJO_CD_QUEUE_CAPACITY];
    size_t queue_head;
    size_t queue_count;
    JojoCdEvent events[EVENT_CAPACITY];
    size_t event_head;
    size_t event_count;
    uint64_t now;
    uint64_t hold_until;     /* no queued request is dispatched before this frame (the drive library is busy) */
    uint8_t ds_mode;         /* the last mode byte sent to the drive (0 after DsInit) */
    int read_active;
    uint32_t read_lba;
    uint32_t read_token;
    uint64_t read_started_frame;
    int alarm_active;
    int ready_repeat;
    uint8_t read_mode;       /* mode byte the next READ takes (set by the adapter before submit) */
    unsigned read_rate4;     /* sectors per frame, in quarters: 10 = 2.5 (double speed), 5 = 1.25 */
    unsigned read_credit4;   /* carried fraction of the sector rate, in quarters */
    uint32_t ready_token;
    uint64_t alarm_due_frame;
    uint64_t alarm_requested_frame;
    uint32_t alarm_token;
    XaEntry *xa_entries;
    size_t xa_count;
    int xa_loaded;
    char error[256];
};

static void set_error(JojoCdStream *s, const char *message) {
    if (!s) return;
    snprintf(s->error, sizeof s->error, "%s", message ? message : "CD stream error");
}

const char *jojo_cd_stream_error(const JojoCdStream *s) {
    return s ? s->error : "CD stream is not initialized";
}

static char *copy_string(const char *s) {
    size_t n;
    char *copy;
    if (!s) return NULL;
    n = strlen(s) + 1;
    copy = (char *)malloc(n);
    if (copy) memcpy(copy, s, n);
    return copy;
}

JojoCdStream *jojo_cd_stream_open(const char *data_root, const char *xa_root) {
    JojoCdStream *s;
    if (!data_root || !*data_root) return NULL;
    s = (JojoCdStream *)calloc(1, sizeof *s);
    if (!s) return NULL;
    s->files = jfl_open(data_root);
    if (!s->files) {
        set_error(s, jfl_error());
        free(s);
        return NULL;
    }
    if (xa_root && *xa_root) {
        s->xa_root = copy_string(xa_root);
        if (!s->xa_root) {
            set_error(s, "out of memory copying XA directory");
            jfl_close(s->files);
            free(s);
            return NULL;
        }
    }
    return s;
}

void jojo_cd_stream_close(JojoCdStream *s) {
    if (!s) return;
    jfl_close(s->files);
    free(s->xa_root);
    free(s->xa_entries);
    free(s);
}

void jojo_cd_stream_reset(JojoCdStream *s) {
    if (!s) return;
    s->queue_head = 0;
    s->queue_count = 0;
    s->event_head = 0;
    s->event_count = 0;
    s->read_active = 0;
    s->alarm_active = 0;
    s->ready_repeat = 0;
    s->ready_token = 0;
    /* hold_until survives a reset: a libds flush does not end the start-up or a speed-change wait */
}

void jojo_cd_stream_ds_init(JojoCdStream *s, uint64_t frame) {
    if (!s) return;
    s->hold_until = frame + JOJO_CD_DS_STARTUP_FRAMES;
    s->ds_mode = 0;
}

/* libds sends a mode byte (Setmode, or the Setmode inside a ReadN packet); a change of the speed bit keeps it busy. */
static void send_mode(JojoCdStream *s, uint8_t mode, uint64_t frame) {
    if ((mode ^ s->ds_mode) & 0x80u) s->hold_until = frame + JOJO_CD_DS_SPEED_FRAMES;
    s->ds_mode = mode;
}

void jojo_cd_stream_ready_set(JojoCdStream *s, int on, uint32_t token) {
    if (!s) return;
    s->ready_repeat = on ? 1 : 0;
    s->ready_token = on ? token : 0;
}

long jojo_cd_stream_read_index(JojoCdStream *s, size_t table_index,
                               uint64_t offset, void *dst, size_t len) {
    long result;
    if (!s || (!dst && len)) return -1;
    result = jfl_read_index(s->files, table_index, offset, dst, len);
    if (result < 0) set_error(s, jfl_error());
    return result;
}

void jojo_cd_stream_set_read_mode(JojoCdStream *s, uint8_t mode) {
    if (s) s->read_mode = mode;
}

int jojo_cd_stream_submit(JojoCdStream *s, JojoCdOperation op,
                          uint8_t command, uint32_t lba,
                          uint32_t delay_frames, uint32_t token) {
    size_t tail;
    CdRequest *r;
    if (!s || op < JOJO_CD_OP_CONTROL || op > JOJO_CD_OP_XA) return 0;
    if (s->queue_count == JOJO_CD_QUEUE_CAPACITY) return 0;
    tail = (s->queue_head + s->queue_count) % JOJO_CD_QUEUE_CAPACITY;
    r = &s->queue[tail];
    r->operation = op;
    r->command = command;
    r->lba = lba;
    r->delay_frames = delay_frames;
    r->requested_frame = s->now;
    r->token = token;
    r->mode = (op == JOJO_CD_OP_READ || (op == JOJO_CD_OP_CONTROL && command == 14u)) ? s->read_mode : 0;
    ++s->queue_count;
    return 1;
}

static int queue_event(JojoCdStream *s, JojoCdEvent *event) {
    size_t tail;
    if (s->event_count == EVENT_CAPACITY) {
        set_error(s, "native CD event queue overflow");
        return 0;
    }
    tail = (s->event_head + s->event_count) % EVENT_CAPACITY;
    s->events[tail] = *event;
    ++s->event_count;
    return 1;
}

static void push_simple_event(JojoCdStream *s, JojoCdEventType type,
                              const CdRequest *request, uint64_t frame,
                              int result) {
    JojoCdEvent event;
    memset(&event, 0, sizeof event);
    event.type = type;
    event.token = request->token;
    event.lba = request->lba;
    event.command = request->command;
    event.result = result;
    event.requested_frame = request->requested_frame;
    event.frame = frame;
    (void)queue_event(s, &event);
}

static int safe_xa_name(const char *name) {
    return name && *name && strlen(name) < XA_NAME_BYTES &&
           !strchr(name, '/') && !strchr(name, '\\') && !strstr(name, "..");
}

static int load_xa_index(JojoCdStream *s) {
    char *path;
    size_t n;
    FILE *fp;
    char line[512];
    size_t cap = 0;
    if (s->xa_loaded) return 1;
    s->xa_loaded = 1;
    if (!s->xa_root) {
        set_error(s, "XA directory was not configured");
        return 0;
    }
    n = strlen(s->xa_root) + sizeof "/index.tsv";
    path = (char *)malloc(n);
    if (!path) { set_error(s, "out of memory opening XA index"); return 0; }
    snprintf(path, n, "%s/index.tsv", s->xa_root);
    fp = fopen(path, "r");
    if (!fp) {
        snprintf(s->error, sizeof s->error, "cannot open %s", path);
        free(path);
        return 0;
    }
    free(path);
    while (fgets(line, sizeof line, fp)) {
        char name[XA_NAME_BYTES];
        unsigned sector, lba, file_no, channel, submode, coding, offset;
        int count;
        XaEntry *grown;
        count = sscanf(line, "%63[^\t]\t%u\t%u\t%u\t%u\t%u\t%u\t%u",
                       name, &sector, &lba, &file_no, &channel,
                       &submode, &coding, &offset);
        (void)sector; (void)submode;
        if (count != 8) continue; /* header */
        if (!safe_xa_name(name)) {
            fclose(fp);
            set_error(s, "invalid XA filename in index");
            return 0;
        }
        if (s->xa_count == cap) {
            size_t next = cap ? cap * 2 : 256;
            grown = (XaEntry *)realloc(s->xa_entries, next * sizeof *grown);
            if (!grown) {
                fclose(fp);
                set_error(s, "out of memory reading XA index");
                return 0;
            }
            s->xa_entries = grown;
            cap = next;
        }
        snprintf(s->xa_entries[s->xa_count].file,
                 sizeof s->xa_entries[s->xa_count].file, "%s", name);
        s->xa_entries[s->xa_count].lba = lba;
        s->xa_entries[s->xa_count].channel = channel;
        s->xa_entries[s->xa_count].offset = offset;
        s->xa_entries[s->xa_count].file_no = file_no;
        s->xa_entries[s->xa_count].coding = coding;
        ++s->xa_count;
    }
    fclose(fp);
    return 1;
}

static int read_sector(JojoCdStream *s, uint32_t lba, uint8_t out[JOJO_CD_SECTOR_BYTES]) {
    size_t file_idx;
    uint32_t sector;
    int table_index;
    long got;
    if (!jfl_find_lba(s->files, lba, &file_idx, &sector)) {
        snprintf(s->error, sizeof s->error, "LBA %u is not a copied data sector", lba);
        return 0;
    }
    (void)file_idx;
    table_index = jfl_lba_to_table_index(s->files, lba);
    if (table_index < 0) {
        snprintf(s->error, sizeof s->error, "LBA %u has no game file-table index", lba);
        return 0;
    }
    memset(out, 0, JOJO_CD_SECTOR_BYTES);
    got = jfl_read_index(s->files, (size_t)table_index,
                         (uint64_t)sector * JOJO_CD_SECTOR_BYTES,
                         out, JOJO_CD_SECTOR_BYTES);
    if (got < 0 || got > (long)JOJO_CD_SECTOR_BYTES) {
        snprintf(s->error, sizeof s->error, "cannot read LBA %u: %s", lba, jfl_error());
        return 0;
    }
    return 1;
}

static int fill_xa_event(JojoCdStream *s, const CdRequest *request,
                         uint64_t frame, JojoCdEvent *event) {
    if (!load_xa_index(s)) return 0;
    for (size_t i = 0; i < s->xa_count; ++i) {
        const XaEntry *entry = &s->xa_entries[i];
        if (entry->lba != request->lba || entry->channel != request->command) continue;
        memset(event, 0, sizeof *event);
        event->type = JOJO_CD_EVENT_XA;
        event->token = request->token;
        event->lba = request->lba;
        event->channel = (uint8_t)entry->channel;
        event->result = 1;
        event->requested_frame = request->requested_frame;
        event->frame = frame;
        event->xa_offset = entry->offset;
        snprintf(event->xa_file, sizeof event->xa_file, "%s", entry->file);
        return 1;
    }
    snprintf(s->error, sizeof s->error,
             "no XA payload at LBA %u channel %u", request->lba, request->command);
    return 0;
}

void jojo_cd_stream_tick(JojoCdStream *s, uint64_t frame) {
    CdRequest request;
    JojoCdEvent event;
    if (!s || frame <= s->now) return;
    s->now = frame;

    /* Every queued command whose turn has come completes in this tick: the drive answers a command within a few milliseconds
     * and libds sends the next one from the completion interrupt, so a whole command packet (the XA job's Pause, Setmode,
     * Setfilter, SeekL, ReadS) completes in one frame in the reference. A libds wait (start-up, speed change) stops the batch. */
    while (s->queue_count && frame >= s->hold_until) {
        /* ReadN packet (Pause, Setmode, Setloc, ReadN): a speed change in its Setmode holds the read itself for the wait. */
        if (s->queue[s->queue_head].operation == JOJO_CD_OP_READ && ((s->queue[s->queue_head].mode ^ s->ds_mode) & 0x80u)) {
            send_mode(s, s->queue[s->queue_head].mode, frame);
            break;
        }
        request = s->queue[s->queue_head];
        s->queue_head = (s->queue_head + 1) % JOJO_CD_QUEUE_CAPACITY;
        --s->queue_count;
        switch (request.operation) {
            case JOJO_CD_OP_CONTROL:
                if (request.command == 9u) s->read_active = 0;
                if (request.command == 14u) send_mode(s, request.mode, frame);
                push_simple_event(s, JOJO_CD_EVENT_CONTROL, &request, frame, 1);
                break;
            case JOJO_CD_OP_READ:
                if (getenv("JOJO_CD_XALOG")) fprintf(stderr, "jojo-cd: READ dispatch lba %u at frame %llu\n", (unsigned)request.lba, (unsigned long long)frame);
                s->ds_mode = request.mode;
                s->read_active = 1;
                s->read_lba = request.lba;
                s->read_token = request.token;
                s->read_started_frame = frame;
                /* Drive rate: 150 sectors/s (double speed, mode bit 0x80) = 2.5 per 60 Hz frame, 75/s = 1.25. The first
                 * sector comes one frame after the READ (the reference's first alarm is at ReadN + 1). */
                s->read_rate4 = (request.mode & 0x80u) ? 10u : 5u;
                s->read_credit4 = 2u;
                push_simple_event(s, JOJO_CD_EVENT_READY, &request, frame, 1);
                break;
            case JOJO_CD_OP_SEEK:
                s->read_active = 0;
                push_simple_event(s, JOJO_CD_EVENT_SEEK, &request, frame, 1);
                break;
            case JOJO_CD_OP_ALARM:
                s->alarm_active = 1;
                s->alarm_due_frame = frame +
                    (request.delay_frames ? request.delay_frames : 1u);
                s->alarm_requested_frame = request.requested_frame;
                s->alarm_token = request.token;
                break;
            case JOJO_CD_OP_XA:
                if (fill_xa_event(s, &request, frame, &event)) {
                    (void)queue_event(s, &event);
                } else {
                    push_simple_event(s, JOJO_CD_EVENT_ERROR, &request, frame, 0);
                }
                break;
            default:
                set_error(s, "unsupported native CD operation");
                push_simple_event(s, JOJO_CD_EVENT_ERROR, &request, frame, 0);
                break;
        }
    }

    /* Sectors of this tick. Each delivered sector is queued as a SECTOR event followed by one ready ALARM (when the ready
     * system is on), so the Capcom alarm copies that sector before the next one arrives, as in the original ready callback. */
    if (s->read_active && frame > s->read_started_frame) {
        unsigned n, i;
        s->read_credit4 += s->read_rate4;
        n = s->read_credit4 / 4u;
        s->read_credit4 %= 4u;
        for (i = 0; i < n; ++i) {
            memset(&event, 0, sizeof event);
            event.type = JOJO_CD_EVENT_SECTOR;
            event.token = s->read_token;
            event.lba = s->read_lba;
            event.result = 1;
            event.requested_frame = frame - 1u;
            event.frame = frame;
            if (!read_sector(s, s->read_lba, event.sector)) {
                event.type = JOJO_CD_EVENT_ERROR;
                event.result = 0;
                s->read_active = 0;
                (void)queue_event(s, &event);
                break;
            }
            ++s->read_lba;
            (void)queue_event(s, &event);
            if (s->ready_repeat) {
                memset(&event, 0, sizeof event);
                event.type = JOJO_CD_EVENT_ALARM;
                event.token = s->ready_token;
                event.command = 1;       /* repeating ready alarm: the token stays bound */
                event.result = 1;
                event.requested_frame = frame;
                event.frame = frame;
                (void)queue_event(s, &event);
            }
        }
    } else if (s->ready_repeat) {
        memset(&event, 0, sizeof event);
        event.type = JOJO_CD_EVENT_ALARM;
        event.token = s->ready_token;
        event.command = 1;               /* repeating ready alarm: the token stays bound */
        event.result = 1;
        event.requested_frame = frame;
        event.frame = frame;
        (void)queue_event(s, &event);
    }

    if (s->alarm_active && frame >= s->alarm_due_frame) {
        memset(&event, 0, sizeof event);
        event.type = JOJO_CD_EVENT_ALARM;
        event.token = s->alarm_token;
        event.result = 1;
        event.requested_frame = s->alarm_requested_frame;
        event.frame = frame;
        (void)queue_event(s, &event);
        s->alarm_active = 0;
    }

}

int jojo_cd_stream_poll(JojoCdStream *s, JojoCdEvent *event) {
    if (!s || !event || !s->event_count) return 0;
    *event = s->events[s->event_head];
    s->event_head = (s->event_head + 1) % EVENT_CAPACITY;
    --s->event_count;
    return 1;
}

int jojo_cd_stream_reading(const JojoCdStream *s) {
    return s ? s->read_active : 0;
}

size_t jojo_cd_stream_pending(const JojoCdStream *s) {
    return s ? s->queue_count : 0;
}

int jojo_cd_stream_read_xa_payload(JojoCdStream *s, const JojoCdEvent *event,
                                   uint8_t out[JOJO_CD_XA_PAYLOAD_BYTES]) {
    char path[512];
    FILE *fp;
    size_t root_len;
    if (!s || !event || !out || event->type != JOJO_CD_EVENT_XA ||
        !safe_xa_name(event->xa_file) || !s->xa_root) return 0;
    root_len = strlen(s->xa_root);
    if (root_len + strlen(event->xa_file) + 2 > sizeof path) {
        set_error(s, "XA path is too long");
        return 0;
    }
    snprintf(path, sizeof path, "%s/%s", s->xa_root, event->xa_file);
    fp = fopen(path, "rb");
    if (!fp) {
        snprintf(s->error, sizeof s->error, "cannot open %s", path);
        return 0;
    }
    if (fseek(fp, (long)event->xa_offset, SEEK_SET) != 0 ||
        fread(out, 1, JOJO_CD_XA_PAYLOAD_BYTES, fp) != JOJO_CD_XA_PAYLOAD_BYTES) {
        fclose(fp);
        snprintf(s->error, sizeof s->error, "short XA payload read at %s+%u",
                 event->xa_file, event->xa_offset);
        return 0;
    }
    fclose(fp);
    return 1;
}

/* ---- XA row access for the CD adapter (the index is in disc order: rows are sorted by LBA) ---- */

int jojo_cd_stream_xa_load(JojoCdStream *s) {
    return s ? load_xa_index(s) : 0;
}

size_t jojo_cd_stream_xa_rows(const JojoCdStream *s) {
    return s ? s->xa_count : 0;
}

long jojo_cd_stream_xa_first_at_or_after(JojoCdStream *s, uint32_t lba) {
    size_t lo = 0, hi;
    if (!s || !load_xa_index(s)) return -1;
    hi = s->xa_count;
    while (lo < hi) {                       /* lower bound on the LBA column */
        size_t mid = lo + (hi - lo) / 2;
        if (s->xa_entries[mid].lba < lba) lo = mid + 1;
        else hi = mid;
    }
    return lo < s->xa_count ? (long)lo : -1;
}

int jojo_cd_stream_xa_row(JojoCdStream *s, size_t row, JojoCdXaRow *out) {
    const XaEntry *e;
    if (!s || !out || row >= s->xa_count) return 0;
    e = &s->xa_entries[row];
    out->lba = e->lba;
    out->file_no = (uint8_t)e->file_no;
    out->channel = (uint8_t)e->channel;
    out->coding = (uint8_t)e->coding;
    return 1;
}

int jojo_cd_stream_xa_read(JojoCdStream *s, size_t row, uint8_t out[JOJO_CD_XA_PAYLOAD_BYTES]) {
    const XaEntry *e;
    char path[1024];
    FILE *fp;
    int ok;
    if (!s || !out || !s->xa_root || row >= s->xa_count) return 0;
    e = &s->xa_entries[row];
    if (strlen(s->xa_root) + strlen(e->file) + 2u > sizeof path) { set_error(s, "XA path is too long"); return 0; }
    snprintf(path, sizeof path, "%s/%s", s->xa_root, e->file);
    fp = fopen(path, "rb");
    if (!fp) { snprintf(s->error, sizeof s->error, "cannot open %s", path); return 0; }
    ok = fseek(fp, (long)e->offset, SEEK_SET) == 0 &&
         fread(out, 1, JOJO_CD_XA_PAYLOAD_BYTES, fp) == JOJO_CD_XA_PAYLOAD_BYTES;
    fclose(fp);
    if (!ok) snprintf(s->error, sizeof s->error, "short XA payload read at %s+%u", e->file, e->offset);
    return ok;
}
