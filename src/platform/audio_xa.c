/* CD-XA ADPCM decoder for the native audio path. See audio_xa.h.
 *
 * Plain C99: standard headers only. The arithmetic follows
 * psxrecomp/runtime/src/cdrom.c (xa_decode_sector_4bit_stereo,
 * xa_decode_sector_4bit_mono, xa_resample_to_44100, cd_apply_decode_volume,
 * maybe_deliver_xa_audio) line for line; the test in tools-phase1/audio_test
 * compiles those runtime lines from the runtime source and compares.
 */
#include "audio_xa.h"

/* Filter table: k0[f] and k1[f] for filter f = 0..3 (the runtime masks the
 * filter field with 0x03, so filter 4 is never used). */
static const int32_t k_xa_k0[5] = { 0, 60, 115, 98, 122 };
static const int32_t k_xa_k1[5] = { 0, 0, -52, -55, -60 };

static int16_t clamp16(int32_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

void xa_decoder_reset(XaDecoder *d) {
    if (!d) return;
    d->hist_l[0] = d->hist_l[1] = 0;
    d->hist_r[0] = d->hist_r[1] = 0;
    d->file = 0xFFu;
    d->channel = 0xFFu;
    d->coding = 0xFFu;
    d->active = 0;
}

int xa_coding_info(uint8_t coding, int *stereo, unsigned *rate_hz) {
    unsigned rate_code = (coding >> 2) & 0x03u;
    unsigned depth_code = (coding >> 4) & 0x03u;
    if (depth_code != 0u) return 0;
    if (rate_code == 0u) {
        if (rate_hz) *rate_hz = 37800u;
    } else if (rate_code == 1u) {
        if (rate_hz) *rate_hz = 18900u;
    } else {
        return 0;
    }
    if (stereo) *stereo = (coding & 0x01u) != 0u;
    return 1;
}

/* One 4-bit sample: sign-extended nibble, shifted by the header range, plus
 * the filter prediction from the two previous decoded samples of the channel. */
static int16_t decode_sample(int32_t nibble, int shift, int filter, int32_t h0, int32_t h1) {
    int32_t predict = (k_xa_k0[filter] * h0 + k_xa_k1[filter] * h1 + 32) >> 6;
    int32_t v = nibble * ((int32_t)1 << shift) + predict;
    return clamp16(v);
}

size_t xa_decode_payload(XaDecoder *d, const uint8_t *payload, int stereo, int16_t *out) {
    size_t pair = 0;
    unsigned g, blk, i, n;

    if (!d || !payload || !out) return 0;

    if (stereo) {
        for (g = 0; g < XA_SOUND_GROUPS; g++) {
            const uint8_t *grp = payload + g * XA_GROUP_BYTES;
            for (blk = 0; blk < 4; blk++) {
                uint8_t hdr_l = grp[4 + blk * 2];
                uint8_t hdr_r = grp[4 + blk * 2 + 1];
                int shift_l = 12 - (int)(hdr_l & 0x0Fu);
                int shift_r = 12 - (int)(hdr_r & 0x0Fu);
                int filter_l = (hdr_l >> 4) & 0x03;
                int filter_r = (hdr_r >> 4) & 0x03;
                if (shift_l < 0) shift_l = 0;
                if (shift_r < 0) shift_r = 0;

                for (i = 0; i < 28; i++) {
                    uint8_t b = grp[16 + blk + i * 4];
                    int32_t nib_l = b & 0x0F;
                    int32_t nib_r = (b >> 4) & 0x0F;
                    int16_t sl, sr;
                    if (nib_l >= 8) nib_l -= 16;
                    if (nib_r >= 8) nib_r -= 16;

                    sl = decode_sample(nib_l, shift_l, filter_l, d->hist_l[0], d->hist_l[1]);
                    sr = decode_sample(nib_r, shift_r, filter_r, d->hist_r[0], d->hist_r[1]);
                    d->hist_l[1] = d->hist_l[0];
                    d->hist_l[0] = sl;
                    d->hist_r[1] = d->hist_r[0];
                    d->hist_r[0] = sr;

                    out[pair * 2 + 0] = sl;
                    out[pair * 2 + 1] = sr;
                    pair++;
                }
            }
        }
        return pair;
    }

    for (g = 0; g < XA_SOUND_GROUPS; g++) {
        const uint8_t *grp = payload + g * XA_GROUP_BYTES;
        for (blk = 0; blk < 4; blk++) {
            for (n = 0; n < 2; n++) {
                uint8_t hdr = grp[4 + blk * 2 + n];
                int shift = 12 - (int)(hdr & 0x0Fu);
                int filter = (hdr >> 4) & 0x03;
                if (shift < 0) shift = 0;

                for (i = 0; i < 28; i++) {
                    uint8_t b = grp[16 + blk + i * 4];
                    int32_t nib = n ? ((b >> 4) & 0x0F) : (b & 0x0F);
                    int16_t s;
                    if (nib >= 8) nib -= 16;

                    s = decode_sample(nib, shift, filter, d->hist_l[0], d->hist_l[1]);
                    d->hist_l[1] = d->hist_l[0];
                    d->hist_l[0] = s;

                    out[pair * 2 + 0] = s;
                    out[pair * 2 + 1] = s;
                    pair++;
                }
            }
        }
    }
    d->hist_r[0] = d->hist_l[0];
    d->hist_r[1] = d->hist_l[1];
    return pair;
}

size_t xa_resample_to_44100(const int16_t *in, size_t in_frames, unsigned rate_hz,
                            int16_t *out, size_t max_frames) {
    size_t out_frames = 0;
    size_t in_pos = 0;
    uint32_t phase = 0;

    if (!in || !out || in_frames == 0 || rate_hz == 0u || max_frames == 0) return 0;

    while (in_pos < in_frames && out_frames < max_frames) {
        size_t next_pos = (in_pos + 1 < in_frames) ? in_pos + 1 : in_pos;
        int32_t cur_l = in[in_pos * 2 + 0];
        int32_t cur_r = in[in_pos * 2 + 1];
        int32_t next_l = in[next_pos * 2 + 0];
        int32_t next_r = in[next_pos * 2 + 1];
        /* Runtime: (next - cur) * phase in int. Formed here in uint32 so the
         * overflow case (full-scale step) wraps instead of being undefined. */
        int32_t dl = (int32_t)((uint32_t)(next_l - cur_l) * phase);
        int32_t dr = (int32_t)((uint32_t)(next_r - cur_r) * phase);
        out[out_frames * 2 + 0] = (int16_t)(cur_l + dl / 44100);
        out[out_frames * 2 + 1] = (int16_t)(cur_r + dr / 44100);
        out_frames++;

        phase += rate_hz;
        while (phase >= XA_OUT_RATE_HZ) {
            phase -= XA_OUT_RATE_HZ;
            in_pos++;
        }
    }
    return out_frames;
}

void xa_apply_volume(int16_t *stereo, size_t frames, const uint8_t m[2][2]) {
    size_t i;
    if (!stereo || !m) return;
    if (m[0][0] == 0x80 && m[1][1] == 0x80 && m[0][1] == 0 && m[1][0] == 0) return;   /* identity */
    for (i = 0; i < frames; i++) {
        int32_t l = stereo[i * 2 + 0];
        int32_t r = stereo[i * 2 + 1];
        int32_t lo = ((l * m[0][0]) >> 7) + ((r * m[1][0]) >> 7);
        int32_t ro = ((l * m[0][1]) >> 7) + ((r * m[1][1]) >> 7);
        stereo[i * 2 + 0] = clamp16(lo);
        stereo[i * 2 + 1] = clamp16(ro);
    }
}

long xa_sector_pcm(XaDecoder *d, uint8_t file, uint8_t channel, uint8_t coding,
                   const uint8_t *payload, int16_t *out) {
    int stereo = 0;
    unsigned rate_hz = 0;
    int16_t native[XA_FRAMES_MONO * 2];
    size_t nframes, nout;

    if (!d || !payload || !out) return -1;
    if (!xa_coding_info(coding, &stereo, &rate_hz)) return -1;

    if (!d->active || d->file != file || d->channel != channel || d->coding != coding) {
        xa_decoder_reset(d);
        d->file = file;
        d->channel = channel;
        d->coding = coding;
        d->active = 1;
    }

    nframes = xa_decode_payload(d, payload, stereo, native);
    nout = xa_resample_to_44100(native, nframes, rate_hz, out, XA_MAX_OUT_FRAMES);
    return (long)nout;
}
