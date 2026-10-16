/* Native CD data stream used by the Phase 1 hybrid adapter.
 *
 * This layer is plain C99: it knows extracted file indexes, sectors and XA
 * payload offsets, but it has no CPUState, guest address or emulator types.
 */
#ifndef JOJO_CD_STREAM_H
#define JOJO_CD_STREAM_H

#include <stddef.h>
#include <stdint.h>

#define JOJO_CD_SECTOR_BYTES 2048u
#define JOJO_CD_XA_PAYLOAD_BYTES 2304u
#define JOJO_CD_QUEUE_CAPACITY 8u

typedef struct JojoCdStream JojoCdStream;

typedef enum {
    JOJO_CD_OP_CONTROL = 1,
    JOJO_CD_OP_READ,
    JOJO_CD_OP_SEEK,
    JOJO_CD_OP_ALARM,
    JOJO_CD_OP_XA
} JojoCdOperation;

typedef enum {
    JOJO_CD_EVENT_CONTROL = 1,
    JOJO_CD_EVENT_SECTOR,
    JOJO_CD_EVENT_SEEK,
    JOJO_CD_EVENT_ALARM,
    JOJO_CD_EVENT_XA,
    JOJO_CD_EVENT_ERROR,
    JOJO_CD_EVENT_READY      /* ReadN accepted: the Capcom ReadN-ready callback with status 2 */
} JojoCdEventType;

typedef struct {
    JojoCdEventType type;
    uint32_t token;              /* opaque caller token; never a guest address */
    uint32_t lba;
    uint8_t command;
    uint8_t channel;
    int result;
    uint64_t requested_frame;
    uint64_t frame;
    uint32_t xa_offset;
    char xa_file[64];
    uint8_t sector[JOJO_CD_SECTOR_BYTES];
} JojoCdEvent;

/* Opens <data_root>/index.tsv. The XA index is loaded lazily from
 * <xa_root>/index.tsv when an XA request arrives. */
JojoCdStream *jojo_cd_stream_open(const char *data_root, const char *xa_root);
void jojo_cd_stream_close(JojoCdStream *stream);
void jojo_cd_stream_reset(JojoCdStream *stream);
const char *jojo_cd_stream_error(const JojoCdStream *stream);

/* Direct file-layer read by the game's file-table index. Returns byte count
 * (short at EOF) or -1 on failure. */
long jojo_cd_stream_read_index(JojoCdStream *stream, size_t table_index,
                               uint64_t offset, void *dst, size_t len);

/* Queue one native operation. Returns 1 when accepted, 0 when the eight-slot
 * command queue is full. A READ starts a continuous one-sector-per-tick stream;
 * a CONTROL command 9 pauses it. Other control values are left to the adapter. */
/* Ready system (libds DsStartReadySystem / DsEndReadySystem): while on, one ALARM event per delivered sector is produced for
 * the registered token (the game alarm callback copies one sector per call). Without a live READ, one per tick. */
void jojo_cd_stream_ready_set(JojoCdStream *stream, int on, uint32_t token);

/* libds pacing, from the decompiled libds (ida/decomp/README.md, "libds start-up and command pacing"): after DsInit the
 * library sends Getstat, Getstat, GetTN, Getstat (one per vblank) before any queued command; a mode byte whose speed bit
 * (0x80) differs from the last one sent keeps it busy for 3 vblanks. A reset (DsFlush) ends neither wait. */
#define JOJO_CD_DS_STARTUP_FRAMES 5u
#define JOJO_CD_DS_SPEED_FRAMES 3u
void jojo_cd_stream_ds_init(JojoCdStream *s, uint64_t frame);
int jojo_cd_stream_submit(JojoCdStream *stream, JojoCdOperation operation,
                          uint8_t command, uint32_t lba,
                          uint32_t delay_frames, uint32_t token);

/* Mode byte of the next READ (ReadN mode, bit 0x80 = double speed) or Setmode (CONTROL 14). Set before submitting it. */
void jojo_cd_stream_set_read_mode(JojoCdStream *stream, uint8_t mode);

/* Advance once at each game frame. Every queued operation completes in the tick (a libds wait stops the batch); a live READ
 * produces its sectors at the drive rate (2.5 per frame at double speed, 1.25 at single). */
void jojo_cd_stream_tick(JojoCdStream *stream, uint64_t frame);
int jojo_cd_stream_poll(JojoCdStream *stream, JojoCdEvent *event);
size_t jojo_cd_stream_pending(const JojoCdStream *stream);
/* 1 while a READ streams sectors (the drive status reading bit 0x20). */
int jojo_cd_stream_reading(const JojoCdStream *stream);

/* Resolve and read one raw 2,304-byte XA payload described by an XA event.
 * Returns 1 on success. The audio engine is the consumer of these requests. */
int jojo_cd_stream_read_xa_payload(JojoCdStream *stream,
                                   const JojoCdEvent *event,
                                   uint8_t out[JOJO_CD_XA_PAYLOAD_BYTES]);

/* XA rows (the audio sectors of extracted_xa/index.tsv, in LBA order). The index loads on the first call. */
typedef struct {
    uint32_t lba;
    uint8_t  file_no;      /* XA subheader file number */
    uint8_t  channel;      /* XA subheader channel */
    uint8_t  coding;       /* XA subheader coding byte */
} JojoCdXaRow;

int    jojo_cd_stream_xa_load(JojoCdStream *stream);
size_t jojo_cd_stream_xa_rows(const JojoCdStream *stream);
long   jojo_cd_stream_xa_first_at_or_after(JojoCdStream *stream, uint32_t lba);  /* row index, or -1 */
int    jojo_cd_stream_xa_row(JojoCdStream *stream, size_t row, JojoCdXaRow *out);
int    jojo_cd_stream_xa_read(JojoCdStream *stream, size_t row, uint8_t out[JOJO_CD_XA_PAYLOAD_BYTES]);

#endif
