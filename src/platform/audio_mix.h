/* SPU voice mixer for the native audio path.
 *
 * Plain C99: standard headers only, no PS1 runtime types, no guest
 * addresses, no global state. All voice state lives in MixCtx, which the
 * caller owns and passes to every call.
 *
 * What it models:
 *   - 24 voices, each playing one SPU-ADPCM sample body (ADPCM blocks are
 *     decoded with audio_adpcm.h as the voice advances)
 *   - pitch counter: 12-bit fraction, pitch 0x1000 = one sample per output
 *     frame (44100 Hz), register value masked to 14 bits
 *   - ADSR envelope (attack, decay, sustain, release) with the SPU rate
 *     tables, linear and exponential modes, stepped once per output frame
 *   - direct-mode stereo volume per voice and a main stereo volume
 *   - END / REPEAT / START block flags, loop address latched from START
 *
 * What it does not model yet (open items, see the report):
 *   - Gaussian interpolation (samples are taken without interpolation)
 *   - sweep-mode volume (bit 15 of a volume register); such a value is refused
 *   - SPU reverb, noise, CD input and the capture buffers
 */
#ifndef JOJO_AUDIO_MIX_H
#define JOJO_AUDIO_MIX_H

#include <stddef.h>
#include <stdint.h>

#include "audio_adpcm.h"

#define MIX_VOICES        24u
#define MIX_RATE_HZ       44100u
#define MIX_PITCH_UNIT    0x1000u   /* pitch 0x1000 = native sample rate */

typedef struct {
    /* Source: ADPCM bytes of one sample, whole 16-byte blocks. */
    const uint8_t *data;
    uint32_t data_bytes;
    uint32_t start;         /* byte offset of the first block */
    uint32_t origin;        /* == start, for mix_voice_position */
    uint32_t cur;           /* byte offset of the next block to decode */
    uint32_t repeat;        /* loop address, byte offset */
    AdpcmState hist;
    int16_t  samples[ADPCM_BLOCK_SAMPLES];
    uint32_t idx;           /* next sample in samples[]; may reach 31 before a decode */
    uint8_t  flags;         /* flags of the last decoded block */
    uint8_t  active;

    /* Pitch */
    uint32_t pitch;         /* 0..0x3FFF */
    uint32_t phase;         /* fraction of one sample, 0..0xFFF */

    /* ADSR */
    uint32_t adsr;          /* SPU ADSR: low half | high half << 16 */
    uint16_t env;           /* envelope level, 0..0x7FFF */
    uint32_t env_div;       /* envelope rate divider */
    uint8_t  env_phase;     /* MIX_ADSR_* */

    /* Direct-mode volume, effective int16 (register value << 1) */
    int16_t  vol_l;
    int16_t  vol_r;
} MixVoice;

/* CD input bus (XA music): a FIFO of decoded 44,100 Hz stereo frames, mixed with the CD-in gain
 * before the 16-bit clamp, as the SPU does (mix order: voices, CD, clamp, main volume). */
#define MIX_CD_FRAMES 16384u

typedef struct {
    MixVoice voice[MIX_VOICES];
    int16_t  main_l;
    int16_t  main_r;
    int16_t  cd_gain_l;     /* CD-in volume register value, 0x7FFF = unity; SsSetSerialVol gives 258 * level */
    int16_t  cd_gain_r;
    int16_t  cd[MIX_CD_FRAMES * 2];
    size_t   cd_r, cd_n;    /* FIFO read position and fill, frames */
    int      cd_started;    /* set by the first push: underflow counts from then on */
    uint32_t cd_underflow;  /* frames rendered with the FIFO empty */
    uint32_t cd_dropped;    /* frames discarded because the FIFO was full */
} MixCtx;

void mix_init(MixCtx *c);

/* Key a voice on, starting at byte offset start of data (data_bytes long).
 * repeat is the loop address (REG7). Returns 0 on success, or a negative code:
 *   -1 voice index out of range or data NULL
 *   -2 start or repeat not a multiple of 16, or start beyond data_bytes
 *   -3 volume register in sweep mode (bit 15 set): not supported */
int  mix_key_on(MixCtx *c, unsigned voice, const uint8_t *data, uint32_t data_bytes,
                uint32_t start, uint32_t repeat,
                uint16_t adsr_lo, uint16_t adsr_hi, uint16_t pitch,
                uint16_t vol_l, uint16_t vol_r);

void mix_key_off(MixCtx *c, unsigned voice);

void mix_set_pitch(MixCtx *c, unsigned voice, uint16_t raw);

/* Returns 0 on success, -1 on a bad voice index, -3 for sweep mode. */
int  mix_set_volume(MixCtx *c, unsigned voice, uint16_t raw_l, uint16_t raw_r);

/* Main volume, direct mode. Bit 15 (sweep mode) is masked off because sweep
 * mode is not modelled, so the value is always decoded as direct mode. */
void mix_set_main_volume(MixCtx *c, uint16_t raw_l, uint16_t raw_r);

/* Sample position of the voice: the index, in its sample body, of the
 * sample the next output frame reads. Only meaningful before any loop has
 * happened (a loop makes the position jump back). */
uint32_t mix_voice_position(const MixVoice *v);

/* Render frames of interleaved int16 stereo (L, R) into out, which must hold
 * 2 * frames values. */
/* CD input bus. mix_cd_push appends frames (dropping the newest when full); mix_cd_pending
 * is the number of frames waiting; mix_set_cd_gain sets the CD-in volume register (signed 16 bit). */
void   mix_cd_push(MixCtx *c, const int16_t *stereo, size_t frames);
size_t mix_cd_pending(const MixCtx *c);
void   mix_set_cd_gain(MixCtx *c, int16_t raw_l, int16_t raw_r);

void mix_render(MixCtx *c, int16_t *out, size_t frames);

#define MIX_ADSR_ATTACK  0u
#define MIX_ADSR_DECAY   1u
#define MIX_ADSR_SUSTAIN 2u
#define MIX_ADSR_RELEASE 3u

#endif
