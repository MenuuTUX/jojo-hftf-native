#include "audio_vab.h"

#define VAB_HDR_BYTES   0x20u
#define VAB_PROG_OFF    0x20u
#define VAB_PROG_BYTES  16u
#define VAB_TONE_OFF    0x820u
#define VAB_TONE_BYTES  32u

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t align_up(uint32_t v, uint32_t a) {
    return (v + a - 1u) / a * a;
}

int vab_parse(VabBank *bank, const uint8_t *buf, size_t len) {
    uint32_t ps, vs, tone_table_end, size_table_off, body_end;
    uint32_t i, k;
    uint32_t sum = 0;

    if (!bank || !buf) return VAB_ERR_ARG;
    if (len < VAB_HDR_BYTES) return VAB_ERR_SHORT;
    if (buf[0] != 'p' || buf[1] != 'B' || buf[2] != 'A' || buf[3] != 'V')
        return VAB_ERR_MAGIC;

    bank->fsize = rd32(buf + 0x0C);
    ps = rd16(buf + 0x12);
    bank->ts = rd16(buf + 0x14);
    vs = rd16(buf + 0x16);
    bank->ps = (uint16_t)ps;
    bank->vs = (uint16_t)vs;
    bank->mvol = buf[0x18];
    bank->pan = buf[0x19];

    if (ps > VAB_MAX_PROGS || vs > VAB_MAX_VAGS) return VAB_ERR_RANGE;

    size_table_off = VAB_TONE_OFF + VAB_TONE_BYTES * VAB_TONES_PER_PROG * ps;
    tone_table_end = size_table_off + 2u * (VAB_MAX_VAGS + 1u);
    if (len < tone_table_end) return VAB_ERR_SHORT;

    for (i = 0; i < VAB_MAX_PROGS; i++) {
        const uint8_t *p = buf + VAB_PROG_OFF + VAB_PROG_BYTES * i;
        VabProg *pg = &bank->prog[i];
        pg->tones = p[0];
        pg->vol = p[1];
        pg->prior = p[2];
        pg->mode = p[3];
        pg->pan = p[4];
        pg->attr = (int16_t)rd16(p + 6);
    }

    for (i = 0; i < VAB_MAX_TONE_SLOTS; i++) {
        VabTone *t = &bank->tone[i];
        if (i < ps * VAB_TONES_PER_PROG) {
            const uint8_t *p = buf + VAB_TONE_OFF + VAB_TONE_BYTES * i;
            t->prior = p[0];
            t->mode = p[1];
            t->vol = p[2];
            t->pan = p[3];
            t->center = p[4];
            t->shift = p[5];
            t->min = p[6];
            t->max = p[7];
            t->vib_w = p[8];
            t->vib_r = p[9];
            t->por_w = p[10];
            t->por_r = p[11];
            t->pb_min = p[12];
            t->pb_max = p[13];
            t->adsr1 = rd16(p + 16);
            t->adsr2 = rd16(p + 18);
            t->prog = (int16_t)rd16(p + 20);
            t->vag = (int16_t)rd16(p + 22);
        } else {
            t->prior = t->mode = t->vol = t->pan = 0;
            t->center = t->shift = t->min = t->max = 0;
            t->vib_w = t->vib_r = t->por_w = t->por_r = 0;
            t->pb_min = t->pb_max = 0;
            t->adsr1 = t->adsr2 = 0;
            t->prog = t->vag = 0;
        }
    }

    bank->tables_end = tone_table_end;
    bank->body_off = align_up(tone_table_end, VAB_BODY_ALIGN);
    bank->vag_off[0] = 0;
    bank->vag_bytes[0] = 0;
    for (k = 1; k <= VAB_MAX_VAGS; k++) {
        uint32_t bytes = (k <= vs) ? 8u * rd16(buf + size_table_off + 2u * k) : 0u;
        bank->vag_bytes[k] = bytes;
        bank->vag_off[k] = (k <= vs) ? bank->body_off + sum : 0u;
        sum += bytes;
    }
    bank->body_bytes = sum;
    body_end = bank->body_off + sum;
    bank->body_end = body_end;
    if (len < body_end) return VAB_ERR_SHORT;
    return VAB_OK;
}

const VabTone *vab_tone(const VabBank *bank, unsigned prog, unsigned k) {
    if (!bank || prog >= bank->ps || prog >= VAB_MAX_PROGS) return NULL;
    if (k >= bank->prog[prog].tones || k >= VAB_TONES_PER_PROG) return NULL;
    return &bank->tone[prog * VAB_TONES_PER_PROG + k];
}
