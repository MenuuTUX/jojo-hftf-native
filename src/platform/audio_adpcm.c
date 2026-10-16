#include "audio_adpcm.h"

static const int32_t adpcm_filter_k0[5] = { 0, 60, 115, 98, 122 };
static const int32_t adpcm_filter_k1[5] = { 0, 0, -52, -55, -60 };

static int16_t adpcm_clamp16(int32_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

void adpcm_state_reset(AdpcmState *st) {
    st->hist1 = 0;
    st->hist2 = 0;
}

uint8_t adpcm_decode_block(AdpcmState *st, const uint8_t *block, int16_t *out) {
    int32_t shift = block[0] & 0x0Fu;
    uint32_t filter = (block[0] >> 4) & 0x0Fu;
    int n_out = 0;

    if (filter > 4u) filter = 0u;
    if (shift > 12) shift = 12;

    for (int b = 0; b < 14; b++) {
        uint8_t packed = block[2 + b];
        for (int n = 0; n < 2; n++) {
            int32_t nib = (n == 0) ? (packed & 0x0F) : (packed >> 4);
            int32_t s;
            if (nib & 0x08) nib -= 0x10;

            s = (nib * 4096) >> shift;
            s += (st->hist1 * adpcm_filter_k0[filter] +
                  st->hist2 * adpcm_filter_k1[filter] + 32) >> 6;
            s = adpcm_clamp16(s);

            st->hist2 = st->hist1;
            st->hist1 = (int16_t)s;
            out[n_out++] = (int16_t)s;
        }
    }
    return block[1];
}

size_t adpcm_decode_run(AdpcmState *st, const uint8_t *data, size_t nbytes,
                        int16_t *out, size_t out_cap) {
    size_t blocks = nbytes / ADPCM_BLOCK_BYTES;
    size_t written = 0;
    for (size_t i = 0; i < blocks; i++) {
        if (out_cap - written < ADPCM_BLOCK_SAMPLES) break;
        adpcm_decode_block(st, data + i * ADPCM_BLOCK_BYTES, out + written);
        written += ADPCM_BLOCK_SAMPLES;
    }
    return written;
}
