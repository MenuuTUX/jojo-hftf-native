#include <string.h>

#include "audio_mix.h"

#define MIX_ENV_MAX   0x7FFFu

static int16_t mix_clamp16(int32_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

/* Direct-mode volume: bit 15 clear, the 15-bit signed value is shifted up
 * one bit to give the effective int16 gain. Full gain (+1.0) is 0x7FFE. */
static int16_t vol_direct(uint16_t raw) {
    return (int16_t)(uint16_t)(raw << 1);
}

/* Envelope rate decoder, ported from the runtime reference calc_vc_delta
 * (itself a port of Beetle's CalcVCDelta). Returns the per-step increment
 * and the divider increment. The level is updated only when the divider
 * crosses 0x8000. */
static void calc_vc_delta(uint8_t zs, uint8_t speed, int log_mode, int dec_mode,
                          int inv_increment, int16_t current,
                          int32_t *out_increment, int32_t *out_divinco)
{
    int32_t increment = 7 - (speed & 0x3);
    int32_t divinco = 32768;

    if (inv_increment) increment = ~increment;

    if (speed < 0x2C)
        increment = (int32_t)((uint32_t)increment << ((0x2F - speed) >> 2));
    if (speed >= 0x30)
        divinco >>= (speed - 0x2C) >> 2;

    if (log_mode) {
        if (dec_mode) {
            increment = (current * increment) >> 15;
        } else if ((current & 0x7FFF) >= 0x6000) {
            if (speed < 0x28) {
                increment >>= 2;
            } else if (speed >= 0x2C) {
                divinco >>= 2;
            } else {
                increment >>= 1;
                divinco >>= 1;
            }
        }
    }

    if (divinco == 0 && speed < zs) divinco = 1;

    *out_increment = increment;
    *out_divinco = divinco;
}

/* One envelope step, once per output frame. The ADSR register fields are
 * read from v->adsr with the SPU bit layout:
 *   low half:  bits 0-3 sustain level, 4-7 decay shift, 8-14 attack rate,
 *              15 attack exponential
 *   high half: bits 0-4 release shift, 5 release exponential, 6-12 sustain
 *              rate, 14 sustain decrease, 15 sustain exponential */
static void adsr_step(MixVoice *v) {
    uint32_t raw = v->adsr;
    int Sl          = (int)(raw & 0x0Fu);
    int Dr          = (int)((raw >> 4) & 0x0Fu);
    int Ar          = (int)((raw >> 8) & 0x7Fu);
    int attack_exp  = (int)((raw >> 15) & 1u);
    int Rr          = (int)((raw >> 16) & 0x1Fu);
    int release_exp = (int)((raw >> 21) & 1u);
    int Sr          = (int)((raw >> 22) & 0x7Fu);
    int sustain_dec = (int)((raw >> 30) & 1u);
    int sustain_exp = (int)((raw >> 31) & 1u);
    int32_t sustain_lvl = (int32_t)(Sl + 1) << 11;
    int32_t increment = 0, divinco = 0, uoflow_reset = 0;

    /* Attack tops out at 0x7FFF and switches to decay before the step. */
    if (v->env_phase == MIX_ADSR_ATTACK && v->env == MIX_ENV_MAX)
        v->env_phase = MIX_ADSR_DECAY;

    switch (v->env_phase) {
    case MIX_ADSR_ATTACK:
        calc_vc_delta(0x7F, (uint8_t)Ar, attack_exp, 0, 0,
                      (int16_t)v->env, &increment, &divinco);
        uoflow_reset = 0x7FFF;
        break;
    case MIX_ADSR_DECAY:
        calc_vc_delta(0x1F << 2, (uint8_t)(Dr << 2), 1, 1, 1,
                      (int16_t)v->env, &increment, &divinco);
        uoflow_reset = 0;
        break;
    case MIX_ADSR_SUSTAIN:
        calc_vc_delta(0x7F, (uint8_t)Sr, sustain_exp, sustain_dec, sustain_dec,
                      (int16_t)v->env, &increment, &divinco);
        uoflow_reset = sustain_dec ? 0 : 0x7FFF;
        break;
    case MIX_ADSR_RELEASE:
        calc_vc_delta(0x1F << 2, (uint8_t)(Rr << 2), release_exp, 1, 1,
                      (int16_t)v->env, &increment, &divinco);
        uoflow_reset = 0;
        break;
    default:
        return;
    }

    v->env_div += (uint32_t)divinco;
    if (v->env_div & 0x8000u) {
        uint16_t prev = v->env;
        v->env_div = 0;
        v->env = (uint16_t)((int32_t)v->env + increment);

        if (v->env_phase == MIX_ADSR_ATTACK) {
            /* Bit 15 just rolled over from 0 to 1: clamp at the top. */
            if (((prev ^ v->env) & v->env) & 0x8000u)
                v->env = (uint16_t)uoflow_reset;
        } else if (v->env & 0x8000u) {
            v->env = (uint16_t)uoflow_reset;
        }

        if (v->env_phase == MIX_ADSR_DECAY && v->env < (uint16_t)sustain_lvl)
            v->env_phase = MIX_ADSR_SUSTAIN;
    }
}

/* Move to the next ADPCM block once the current one is used up. Returns 0
 * when the voice must stop: an END block without REPEAT, or no more data. */
static int voice_advance_block(MixVoice *v) {
    const uint8_t *blk;

    if (v->flags & ADPCM_FLAG_END) {
        if (!(v->flags & ADPCM_FLAG_REPEAT)) return 0;
        v->cur = v->repeat;
    }
    if (v->cur > v->data_bytes || v->data_bytes - v->cur < ADPCM_BLOCK_BYTES)
        return 0;

    blk = v->data + v->cur;
    if (blk[1] & ADPCM_FLAG_START) v->repeat = v->cur;
    v->flags = adpcm_decode_block(&v->hist, blk, v->samples);
    v->cur += ADPCM_BLOCK_BYTES;
    v->idx -= ADPCM_BLOCK_SAMPLES;
    return 1;
}

/* One output frame of one voice: pick the sample, apply the envelope, step
 * the envelope and advance the pitch counter. */
static int16_t voice_next(MixVoice *v) {
    int32_t shaped;

    if (!v->active) return 0;
    if (v->idx >= ADPCM_BLOCK_SAMPLES && !voice_advance_block(v)) {
        v->active = 0;
        return 0;
    }
    if (v->idx >= ADPCM_BLOCK_SAMPLES) {    /* unreachable: advance is at most 3 */
        v->active = 0;
        return 0;
    }

    shaped = ((int32_t)v->samples[v->idx] * (int32_t)v->env) >> 15;
    shaped = mix_clamp16(shaped);

    adsr_step(v);
    if (v->env_phase == MIX_ADSR_RELEASE && v->env == 0)
        v->active = 0;

    /* Pitch 0 holds the current sample. Every whole unit of phase is one
     * sample. A step that crosses a block boundary leaves idx at or past 28,
     * and the next call decodes the next block and carries the rest over, so
     * no step is lost or delayed. */
    v->phase += v->pitch;
    while (v->phase >= MIX_PITCH_UNIT) {
        v->phase -= MIX_PITCH_UNIT;
        v->idx++;
    }
    return (int16_t)shaped;
}

void mix_init(MixCtx *c) {
    memset(c, 0, sizeof *c);
    c->main_l = 0x7FFE;
    c->main_r = 0x7FFE;
}

int mix_key_on(MixCtx *c, unsigned voice, const uint8_t *data, uint32_t data_bytes,
               uint32_t start, uint32_t repeat,
               uint16_t adsr_lo, uint16_t adsr_hi, uint16_t pitch,
               uint16_t vol_l, uint16_t vol_r)
{
    MixVoice *v;

    if (!c || voice >= MIX_VOICES || !data) return -1;
    if ((start % ADPCM_BLOCK_BYTES) || (repeat % ADPCM_BLOCK_BYTES) ||
        start >= data_bytes)
        return -2;
    if ((vol_l & 0x8000u) || (vol_r & 0x8000u)) return -3;

    v = &c->voice[voice];
    memset(v, 0, sizeof *v);
    v->data = data;
    v->data_bytes = data_bytes;
    v->start = start;
    v->origin = start;
    v->cur = start;
    v->repeat = repeat;
    adpcm_state_reset(&v->hist);
    v->idx = ADPCM_BLOCK_SAMPLES;      /* first call decodes the first block */
    v->flags = 0;
    v->active = 1;
    v->pitch = pitch & 0x3FFFu;
    v->phase = 0;
    v->adsr = (uint32_t)adsr_lo | ((uint32_t)adsr_hi << 16);
    v->env = 0;
    v->env_div = 0;
    v->env_phase = MIX_ADSR_ATTACK;
    v->vol_l = vol_direct(vol_l);
    v->vol_r = vol_direct(vol_r);
    return 0;
}

void mix_key_off(MixCtx *c, unsigned voice) {
    MixVoice *v;

    if (!c || voice >= MIX_VOICES) return;
    v = &c->voice[voice];
    if (!v->active) return;
    if (v->env_phase != MIX_ADSR_RELEASE) {
        v->env_phase = MIX_ADSR_RELEASE;
        v->env_div = 0;
    }
}

void mix_set_pitch(MixCtx *c, unsigned voice, uint16_t raw) {
    if (!c || voice >= MIX_VOICES) return;
    c->voice[voice].pitch = raw & 0x3FFFu;
}

int mix_set_volume(MixCtx *c, unsigned voice, uint16_t raw_l, uint16_t raw_r) {
    if (!c || voice >= MIX_VOICES) return -1;
    if ((raw_l & 0x8000u) || (raw_r & 0x8000u)) return -3;
    c->voice[voice].vol_l = vol_direct(raw_l);
    c->voice[voice].vol_r = vol_direct(raw_r);
    return 0;
}

void mix_set_main_volume(MixCtx *c, uint16_t raw_l, uint16_t raw_r) {
    if (!c) return;
    c->main_l = vol_direct((uint16_t)(raw_l & 0x7FFFu));
    c->main_r = vol_direct((uint16_t)(raw_r & 0x7FFFu));
}

uint32_t mix_voice_position(const MixVoice *v) {
    int64_t blocks, pos;

    if (v->cur < v->origin) return 0u;
    blocks = (int64_t)((v->cur - v->origin) / ADPCM_BLOCK_BYTES);
    pos = blocks * ADPCM_BLOCK_SAMPLES - ADPCM_BLOCK_SAMPLES + (int64_t)v->idx;
    return pos < 0 ? 0u : (uint32_t)pos;
}

void mix_cd_push(MixCtx *c, const int16_t *stereo, size_t frames) {
    size_t i;
    if (!c || !stereo) return;
    c->cd_started = 1;
    for (i = 0; i < frames; i++) {
        size_t w;
        if (c->cd_n == MIX_CD_FRAMES) {
            c->cd_dropped++;
            continue;
        }
        w = (c->cd_r + c->cd_n) % MIX_CD_FRAMES;
        c->cd[2 * w] = stereo[2 * i];
        c->cd[2 * w + 1] = stereo[2 * i + 1];
        c->cd_n++;
    }
}

size_t mix_cd_pending(const MixCtx *c) {
    return c ? c->cd_n : 0;
}

void mix_set_cd_gain(MixCtx *c, int16_t raw_l, int16_t raw_r) {
    if (!c) return;
    c->cd_gain_l = raw_l;
    c->cd_gain_r = raw_r;
}

void mix_render(MixCtx *c, int16_t *out, size_t frames) {
    size_t f;

    for (f = 0; f < frames; f++) {
        int32_t l = 0;
        int32_t r = 0;
        unsigned i;

        for (i = 0; i < MIX_VOICES; i++) {
            MixVoice *v = &c->voice[i];
            int32_t s = voice_next(v);
            if (!s) continue;
            l += (s * v->vol_l) >> 15;
            r += (s * v->vol_r) >> 15;
        }

        if (c->cd_n) {
            int32_t cl = c->cd[2 * c->cd_r];
            int32_t cr = c->cd[2 * c->cd_r + 1];
            c->cd_r = (c->cd_r + 1) % MIX_CD_FRAMES;
            c->cd_n--;
            l += (cl * c->cd_gain_l) >> 15;
            r += (cr * c->cd_gain_r) >> 15;
        } else if (c->cd_started) {
            c->cd_underflow++;
        }

        l = mix_clamp16(l);
        r = mix_clamp16(r);
        l = mix_clamp16((l * c->main_l) >> 15);
        r = mix_clamp16((r * c->main_r) >> 15);
        out[2 * f] = (int16_t)l;
        out[2 * f + 1] = (int16_t)r;
    }
}
