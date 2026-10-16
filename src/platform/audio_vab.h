/* VAB sound-bank parser for the native audio path.
 *
 * Plain C99: standard headers only, no PS1 runtime types, no guest
 * addresses, no global state. The caller owns the VabBank (it is large,
 * about 50 KB, so allocate it on the heap or as a static in the caller).
 *
 * Layout used (verified against the game's extracted banks, see
 * tools-phase1/audio_test):
 *   0x000  VabHdr, 32 bytes: "pBAV", version, id, fsize, reserved, ps, ts,
 *          vs, mvol, pan, attr1, attr2, reserved
 *   0x020  program table, 128 x 16 bytes (ps used)
 *   0x820  tone table, 16 slots per program, 32 bytes each. The tone for
 *          program p, slot k is at index 16 * p + k; slots past
 *          prog[p].tones are unused.
 *   0x820 + 512 * ps  vag size table, 256 x u16, index = vag number (1..vs),
 *          value in 8-byte units. Index 0 is unused.
 *   body   the SPU-ADPCM sample bodies, starting at the next 0x800
 *          boundary after the size table. Vag k starts at the end of vag
 *          k-1 (vag 1 starts at body_off).
 */
#ifndef JOJO_AUDIO_VAB_H
#define JOJO_AUDIO_VAB_H

#include <stddef.h>
#include <stdint.h>

#define VAB_MAX_PROGS        128u
#define VAB_TONES_PER_PROG   16u
#define VAB_MAX_TONE_SLOTS   (VAB_MAX_PROGS * VAB_TONES_PER_PROG)
#define VAB_MAX_VAGS         255u   /* vag numbers 1..255 */
#define VAB_BODY_ALIGN       0x800u

enum {
    VAB_OK = 0,
    VAB_ERR_ARG = -1,
    VAB_ERR_MAGIC = -2,    /* buffer does not start with "pBAV" */
    VAB_ERR_SHORT = -3,    /* buffer shorter than the header tables or bodies */
    VAB_ERR_RANGE = -4     /* ps or vs above the format limits */
};

typedef struct {
    uint8_t tones;      /* tone slots in use for this program (0..16) */
    uint8_t vol;
    uint8_t prior;
    uint8_t mode;
    uint8_t pan;
    int16_t attr;
} VabProg;

typedef struct {
    uint8_t  prior;
    uint8_t  mode;
    uint8_t  vol;
    uint8_t  pan;
    uint8_t  center;     /* center note, 60 = middle C */
    uint8_t  shift;      /* fine pitch, added to center */
    uint8_t  min;        /* lowest note */
    uint8_t  max;        /* highest note */
    uint8_t  vib_w;
    uint8_t  vib_r;
    uint8_t  por_w;
    uint8_t  por_r;
    uint8_t  pb_min;
    uint8_t  pb_max;
    uint16_t adsr1;      /* SPU ADSR low half (register value) */
    uint16_t adsr2;      /* SPU ADSR high half (register value) */
    int16_t  prog;       /* program index this tone belongs to */
    int16_t  vag;        /* sample number, 1..vs; 0 = none */
} VabTone;

typedef struct {
    uint32_t fsize;          /* header file-size field */
    uint16_t ps;             /* programs */
    uint16_t ts;             /* tones (header count) */
    uint16_t vs;             /* vags (samples) */
    uint8_t  mvol;
    uint8_t  pan;
    uint32_t tables_end;     /* end of header and tables, unpadded: fsize == tables_end + body_bytes */
    uint32_t body_off;       /* byte offset of the first sample body, from VAB start */
    uint32_t body_bytes;     /* sum of all vag sizes, in bytes */
    uint32_t body_end;       /* body_off + body_bytes, from VAB start */
    VabProg  prog[VAB_MAX_PROGS];
    VabTone  tone[VAB_MAX_TONE_SLOTS];
    uint32_t vag_off[VAB_MAX_VAGS + 1];    /* [vag]: start, from VAB start */
    uint32_t vag_bytes[VAB_MAX_VAGS + 1];  /* [vag]: size in bytes */
} VabBank;

/* Parse the VAB that starts at buf[0] and is at most len bytes long. buf
 * must begin with "pBAV". Fills *bank. Returns VAB_OK or a VAB_ERR_* code. */
int vab_parse(VabBank *bank, const uint8_t *buf, size_t len);

/* Tone for program prog, slot k (0 <= k < prog[prog].tones). NULL if out of
 * range. */
const VabTone *vab_tone(const VabBank *bank, unsigned prog, unsigned k);

#endif
