/* SEQ sequence player for the native audio path (replacement for the
 * libsnd sequencer, SsSeq*).
 *
 * Plain C99: standard headers only, no PS1 runtime types, no guest
 * addresses, no global state. The caller owns the SeqPlayer and the sequence
 * bytes, which must stay valid while the player is open.
 *
 * Layout (GUESS: the format is recalled from the PSY-Q sequencer and no
 * sequence in the extracted game files was available to check it; see the
 * report of the adapter piece):
 *   0x00  "pQES"
 *   0x04  u32 big-endian version
 *   0x08  u16 big-endian ticks per quarter note (ppqn, nonzero)
 *   0x0A  u24 big-endian initial tempo, microseconds per quarter note
 *   0x0D  u8 time signature numerator
 *   0x0E  u8 time signature denominator (power of two)
 *   0x0F  track: MIDI-style events. Each event is a variable-length delta
 *         time (in ticks) followed by the event. Running status is allowed.
 *         FF 51 03 sets the tempo, FF 2F ends the track, FF xx and F0/F7
 *         carry a length and are skipped (FF 01 text included).
 *
 * Time is kept in microseconds. An event is due at the due time of the
 * previous event plus delta * tempo / ppqn (integer division per event).
 */
#ifndef JOJO_AUDIO_SEQ_H
#define JOJO_AUDIO_SEQ_H

#include <stddef.h>
#include <stdint.h>

#define SEQ_HEADER_BYTES 15u

enum {
    SEQ_OK = 0,
    SEQ_ERR_ARG = -1,
    SEQ_ERR_MAGIC = -2,    /* buffer does not start with "pQES" */
    SEQ_ERR_SHORT = -3,    /* shorter than the header */
    SEQ_ERR_FORMAT = -4    /* ppqn zero or an unsupported event */
};

typedef struct {
    uint32_t version;
    uint16_t ppqn;
    uint32_t tempo_us;     /* initial tempo */
    uint8_t  ts_num;
    uint8_t  ts_den;
} SeqHeader;

typedef enum {
    SEQ_EV_NOTE_ON = 1,
    SEQ_EV_NOTE_OFF,
    SEQ_EV_PROGRAM,
    SEQ_EV_CONTROL,
    SEQ_EV_PITCH_BEND,    /* bend: 0..0x3FFF, centre 0x2000 */
    SEQ_EV_TEMPO,         /* tempo_us */
    SEQ_EV_END            /* end of track, before any loop restart */
} SeqEventKind;

typedef struct {
    SeqEventKind kind;
    uint8_t  channel;     /* 0..15 */
    uint8_t  key;         /* note, program or controller number */
    uint8_t  value;       /* velocity (scaled by the sequence volume) or controller value */
    uint16_t bend;
    uint32_t tempo_us;
    uint64_t at_us;       /* due time since seq_play, microseconds */
} SeqEvent;

typedef void (*SeqSink)(void *user, const SeqEvent *ev);

typedef struct {
    const uint8_t *buf;
    size_t len;
    SeqHeader hdr;
    SeqSink sink;
    void *user;

    /* playback state, set by seq_play */
    size_t   track_start;
    size_t   pos;
    uint8_t  running;     /* last channel status byte, 0 when none */
    int      playing;
    int      looping;
    int      error;       /* set when a playback error stops the track */
    int      vol;         /* sequence volume 0..127, default 127 */
    uint32_t tempo_us;    /* tempo in effect */
    uint64_t now_us;      /* clock since seq_play */
    uint64_t base_us;     /* due time of the last event read */
    uint64_t ticks;       /* sum of all deltas read since seq_play */
    int      pending;     /* the next event is read and waiting for its due time */
    SeqEvent next;
    uint64_t events;      /* events emitted since seq_play */
} SeqPlayer;

/* Parse the 15-byte header of the sequence at buf[0..len). */
int seq_parse_header(SeqHeader *h, const uint8_t *buf, size_t len);

/* Open a sequence for playing. Sets the player to stopped. */
int seq_open(SeqPlayer *p, const uint8_t *buf, size_t len, SeqSink sink, void *user);

/* Start from the top of the track. loop != 0 restarts the track at its end. */
void seq_play(SeqPlayer *p, int loop);

void seq_stop(SeqPlayer *p);

/* Sequence volume 0..127 applied to note velocities from now on. */
void seq_set_vol(SeqPlayer *p, int vol);

/* Advance the clock by us microseconds and emit every event that is due, in
 * order. Returns the number of events emitted. Call once per vblank with
 * 16667 us (60 Hz) or the tick period the game uses. */
size_t seq_advance(SeqPlayer *p, uint32_t us);

#endif
