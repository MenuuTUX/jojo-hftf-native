/* SPU-ADPCM block decoder for the native audio path.
 *
 * Plain C99: standard headers only, no PS1 runtime types, no guest
 * addresses, no global state. All decoder state is in AdpcmState, which the
 * caller owns.
 *
 * Block layout (16 bytes, 28 samples):
 *   byte 0       high nibble = filter (0..4), low nibble = shift
 *   byte 1       flags: bit 0 END, bit 1 REPEAT, bit 2 START (loop address)
 *   bytes 2..15  28 4-bit samples, low nibble of each byte first
 *
 * The arithmetic is the same as the runtime reference decode_block(): a
 * filter index above 4 is treated as 0 and a shift above 12 is treated as 12.
 * Beetle resets a shift above 12 to 8 instead; that difference is known and
 * not used by any block in the corpus measured by tools-phase1/audio_test.
 */
#ifndef JOJO_AUDIO_ADPCM_H
#define JOJO_AUDIO_ADPCM_H

#include <stddef.h>
#include <stdint.h>

#define ADPCM_BLOCK_BYTES   16u
#define ADPCM_BLOCK_SAMPLES 28u

#define ADPCM_FLAG_END    0x01u
#define ADPCM_FLAG_REPEAT 0x02u
#define ADPCM_FLAG_START  0x04u

/* Two previous decoded samples. Starts at zero for each sample body. */
typedef struct {
    int16_t hist1;
    int16_t hist2;
} AdpcmState;

void adpcm_state_reset(AdpcmState *st);

/* Decode one 16-byte block into 28 int16 samples. Updates st. Returns the
 * block flags (byte 1) so the caller can apply END/REPEAT/START rules. */
uint8_t adpcm_decode_block(AdpcmState *st, const uint8_t *block, int16_t *out);

/* Decode nbytes / 16 whole blocks from data into out (28 samples per block).
 * A trailing partial block is ignored. Stops early if out_cap is too small.
 * Returns the number of samples written. */
size_t adpcm_decode_run(AdpcmState *st, const uint8_t *data, size_t nbytes,
                        int16_t *out, size_t out_cap);

#endif
