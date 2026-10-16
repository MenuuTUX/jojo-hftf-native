/* CD-XA ADPCM decoder for the native audio path.
 *
 * Plain C99: standard headers only, no PS1 runtime types, no guest addresses,
 * no global state. All decoder state is in XaDecoder, which the caller owns.
 *
 * Input: the 2,304-byte payload of one XA audio sector (XA form 2 user area:
 * 18 sound groups of 128 bytes). tools-phase1/extract_xa.py writes exactly
 * these bytes to extracted_xa/JOJO_Mnn.xa (raw sector bytes 24..2327), and the
 * index row gives file, channel and coding for each payload.
 *
 * Sound group (128 bytes), as read by the runtime decoder
 * (psxrecomp/runtime/src/cdrom.c, xa_decode_sector_4bit_stereo/_mono):
 *   bytes 0..3    not read by the decoder
 *   bytes 4..11   filter/shift header bytes, one per (block, channel):
 *                 stereo: byte 4+2*blk = left, 5+2*blk = right
 *                 mono:   byte 4+2*blk+n for the two 14-sample halves n = 0, 1
 *   bytes 16..127 28 rows of 4 bytes; row i, block blk is byte 16+blk+4*i.
 *                 stereo: low nibble = left sample, high nibble = right sample
 *                 mono:   low nibble = half 0, high nibble = half 1
 *
 * Coding byte (the XA subheader byte 3): bit 0 stereo, bits 2..3 rate
 * (0 = 37,800 Hz, 1 = 18,900 Hz), bits 4..5 depth (only 0, 4 bit, is decoded).
 *
 * Pipeline (the order the runtime uses, and the order the CD adapter must
 * keep):
 *   xa_sector_pcm()        decode -> resample to 44,100 Hz
 *   xa_apply_volume()      CD controller decode matrix (0x80 = unity)
 *   SPU CD input gain      the libspu CD-in volume register (0x7FFF = unity),
 *                          applied at the CD mix stage as (s * gain) >> 15
 *
 * Bit-exact to the runtime decoder for every input this module accepts. The
 * one known runtime quirk that is kept on purpose (see xa_resample_to_44100):
 * the resampler restarts at every sector, so each sector's output starts from
 * phase 0 and the last input frame of one sector is not carried into the next.
 */
#ifndef JOJO_AUDIO_XA_H
#define JOJO_AUDIO_XA_H

#include <stddef.h>
#include <stdint.h>

#define XA_PAYLOAD_BYTES     2304u
#define XA_SOUND_GROUPS      18u
#define XA_GROUP_BYTES       128u
#define XA_FRAMES_STEREO     2016u   /* 18 groups * 4 blocks * 28 */
#define XA_FRAMES_MONO       4032u   /* 18 groups * 8 halves * 28 */
#define XA_OUT_RATE_HZ       44100u
#define XA_MAX_OUT_FRAMES    9408u   /* output capacity for one sector at 37,800 Hz */

/* Decoder state. Samples history is per channel: index 0 is the most recent
 * decoded sample, index 1 the one before. Mono decoding uses the left history
 * and copies it to the right at the end of each sector, as the runtime does. */
typedef struct {
    int32_t hist_l[2];
    int32_t hist_r[2];
    uint8_t file;       /* subheader file number of the stream */
    uint8_t channel;    /* subheader channel of the stream */
    uint8_t coding;     /* coding byte of the stream */
    int     active;     /* 0 until the first sector of a stream */
} XaDecoder;

/* Clears the history and the stream identity. */
void xa_decoder_reset(XaDecoder *d);

/* Parses a coding byte. Returns 1 when the sector is decodable (4-bit depth,
 * rate code 0 or 1), 0 otherwise. *stereo is 1 or 0; *rate_hz is 37800 or 18900. */
int xa_coding_info(uint8_t coding, int *stereo, unsigned *rate_hz);

/* Decodes one payload (XA_PAYLOAD_BYTES). out receives interleaved L, R
 * int16 frames: XA_FRAMES_STEREO frames for stereo, XA_FRAMES_MONO for mono
 * (mono is written with equal L and R). Returns the number of frames written.
 * Does not look at the stream identity: use xa_sector_pcm for a stream. */
size_t xa_decode_payload(XaDecoder *d, const uint8_t *payload, int stereo, int16_t *out);

/* Resamples interleaved L, R int16 frames from rate_hz to 44,100 Hz by linear
 * interpolation, restarting at phase 0 for each call. out must hold max_frames
 * frames. Returns the number of output frames. Mirrors the runtime exactly,
 * including the 32-bit product that the runtime leaves to signed overflow
 * (here the product is formed in uint32 and wrapped, which is the same bits
 * on every two's-complement target). */
size_t xa_resample_to_44100(const int16_t *in, size_t in_frames, unsigned rate_hz,
                            int16_t *out, size_t max_frames);

/* Applies the CD decode matrix to interleaved frames. m[src][dst] with 0x80 =
 * unity: out_l = (l*m[0][0] >> 7) + (r*m[1][0] >> 7), out_r = (l*m[0][1] >> 7)
 * + (r*m[1][1] >> 7), each clamped to int16. */
void xa_apply_volume(int16_t *stereo, size_t frames, const uint8_t m[2][2]);

/* One sector of a stream, from payload to 44,100 Hz PCM. Resets the decoder
 * when file, channel or coding differ from the stream (as the runtime does),
 * decodes, and resamples into out (XA_MAX_OUT_FRAMES frames at most). Returns
 * the number of 44,100 Hz frames, or -1 when the coding byte is not decodable
 * (the runtime refuses those sectors too; no state is changed then). */
long xa_sector_pcm(XaDecoder *d, uint8_t file, uint8_t channel, uint8_t coding,
                   const uint8_t *payload, int16_t *out);

#endif
