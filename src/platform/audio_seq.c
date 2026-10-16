/* SEQ sequence player: see audio_seq.h for the layout and its GUESS flags. */
#include <string.h>

#include "audio_seq.h"

#define SEQ_MAX_EVENTS_PER_ADVANCE 1000000u

static uint32_t be16(const uint8_t *p) { return ((uint32_t)p[0] << 8) | p[1]; }
static uint32_t be24(const uint8_t *p) { return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2]; }
static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

int seq_parse_header(SeqHeader *h, const uint8_t *buf, size_t len) {
    if (!h || !buf) return SEQ_ERR_ARG;
    if (len < SEQ_HEADER_BYTES) return SEQ_ERR_SHORT;
    if (buf[0] != 'p' || buf[1] != 'Q' || buf[2] != 'E' || buf[3] != 'S')
        return SEQ_ERR_MAGIC;
    h->version = be32(buf + 4);
    h->ppqn = (uint16_t)be16(buf + 8);
    h->tempo_us = be24(buf + 10);
    h->ts_num = buf[13];
    h->ts_den = buf[14];
    if (h->ppqn == 0 || h->tempo_us == 0) return SEQ_ERR_FORMAT;
    return SEQ_OK;
}

int seq_open(SeqPlayer *p, const uint8_t *buf, size_t len, SeqSink sink, void *user) {
    int rc;
    if (!p || !buf) return SEQ_ERR_ARG;
    rc = seq_parse_header(&p->hdr, buf, len);
    if (rc != SEQ_OK) return rc;
    p->buf = buf;
    p->len = len;
    p->sink = sink;
    p->user = user;
    p->track_start = SEQ_HEADER_BYTES;
    p->playing = 0;
    p->looping = 0;
    p->error = 0;
    p->vol = 127;
    p->pending = 0;
    p->events = 0;
    return SEQ_OK;
}

void seq_stop(SeqPlayer *p) {
    if (!p) return;
    p->playing = 0;
    p->pending = 0;
}

void seq_set_vol(SeqPlayer *p, int vol) {
    if (!p) return;
    if (vol < 0) vol = 0;
    if (vol > 127) vol = 127;
    p->vol = vol;
}

/* Read a MIDI variable-length quantity at p->pos. Returns 0 on success. */
static int read_vlq(SeqPlayer *p, uint32_t *out) {
    uint32_t v = 0;
    int i;
    for (i = 0; i < 4; i++) {
        uint8_t b;
        if (p->pos >= p->len) return -1;
        b = p->buf[p->pos++];
        v = (v << 7) | (uint32_t)(b & 0x7Fu);
        if (!(b & 0x80u)) {
            *out = v;
            return 0;
        }
    }
    return -1;
}

static int read_byte(SeqPlayer *p, uint8_t *out) {
    if (p->pos >= p->len) return -1;
    *out = p->buf[p->pos++];
    return 0;
}

static void fail(SeqPlayer *p) {
    p->error = 1;
    p->playing = 0;
    p->pending = 0;
}

/* Read the next event of the track into p->next and set its due time. The
 * delta is measured from the due time of the previous event, with the tempo
 * in effect now (a tempo change applies to the deltas after it). Returns 0,
 * or -1 when the track is malformed or ends without FF 2F. */
static int read_next(SeqPlayer *p) {
    uint32_t delta;
    uint8_t status, data1 = 0, data2 = 0;
    uint32_t len;
    SeqEvent *ev = &p->next;

    if (p->pos >= p->len) return -1;
    if (read_vlq(p, &delta) != 0) return -1;
    p->ticks += delta;
    memset(ev, 0, sizeof *ev);
    ev->at_us = p->base_us + ((uint64_t)delta * p->tempo_us) / p->hdr.ppqn;

    if (read_byte(p, &status) != 0) return -1;
    if (status & 0x80u) {
        if (status < 0xF0u) p->running = status;
    } else {
        /* data byte: running status, the byte is the first data byte */
        if (p->running == 0) return -1;
        status = p->running;
        p->pos--;
    }

    if (status < 0xF0u) {
        uint8_t kind = status & 0xF0u;
        ev->channel = status & 0x0Fu;
        if (kind == 0xC0u || kind == 0xD0u) {
            if (read_byte(p, &data1) != 0) return -1;
            if (kind == 0xC0u) {
                ev->kind = SEQ_EV_PROGRAM;
                ev->key = data1;
                return 0;
            }
            return 1;    /* channel pressure: consumed, no event */
        }
        if (read_byte(p, &data1) != 0 || read_byte(p, &data2) != 0) return -1;
        switch (kind) {
        case 0x80u:
            ev->kind = SEQ_EV_NOTE_OFF;
            ev->key = data1;
            return 0;
        case 0x90u:
            ev->key = data1;
            if (data2 == 0) {
                ev->kind = SEQ_EV_NOTE_OFF;
            } else {
                ev->kind = SEQ_EV_NOTE_ON;
                ev->value = (uint8_t)((data2 * (uint32_t)p->vol) / 127u);
            }
            return 0;
        case 0xA0u:
            return 1;    /* polyphonic pressure: consumed, no event */
        case 0xB0u:
            ev->kind = SEQ_EV_CONTROL;
            ev->key = data1;
            ev->value = data2;
            return 0;
        case 0xE0u:
            ev->kind = SEQ_EV_PITCH_BEND;
            ev->bend = (uint16_t)(data1 | ((uint32_t)data2 << 7));
            return 0;
        default:
            return -1;
        }
    }

    if (status == 0xFFu) {
        uint8_t type;
        p->running = 0;
        if (read_byte(p, &type) != 0 || read_vlq(p, &len) != 0) return -1;
        if (len > p->len - p->pos) return -1;
        if (type == 0x2Fu) {
            ev->kind = SEQ_EV_END;
            return 0;
        }
        if (type == 0x51u && len == 3) {
            ev->kind = SEQ_EV_TEMPO;
            ev->tempo_us = be24(p->buf + p->pos);
            if (ev->tempo_us == 0) return -1;
            p->pos += len;
            return 0;
        }
        p->pos += len;     /* other meta events: skipped */
        return 1;
    }

    if (status == 0xF0u || status == 0xF7u) {
        p->running = 0;
        if (read_vlq(p, &len) != 0) return -1;
        if (len > p->len - p->pos) return -1;
        p->pos += len;     /* sysex: skipped */
        return 1;
    }
    return -1;
}

/* Read events until one with an output is pending, or the track fails. */
static void fetch(SeqPlayer *p) {
    for (;;) {
        int rc = read_next(p);
        if (rc == 0) {
            p->pending = 1;
            return;
        }
        if (rc < 0) {
            fail(p);
            return;
        }
        /* rc == 1: consumed without an output event; ticks were counted */
        p->base_us = p->next.at_us;
    }
}

void seq_play(SeqPlayer *p, int loop) {
    if (!p || !p->buf) return;
    p->tempo_us = p->hdr.tempo_us;
    p->pos = p->track_start;
    p->running = 0;
    p->playing = 1;
    p->looping = loop ? 1 : 0;
    p->error = 0;
    p->now_us = 0;
    p->base_us = 0;
    p->ticks = 0;
    p->events = 0;
    p->pending = 0;
    fetch(p);
}

size_t seq_advance(SeqPlayer *p, uint32_t us) {
    size_t emitted = 0;
    uint32_t guard = 0;
    if (!p || !p->playing) return 0;
    p->now_us += us;
    while (p->playing && p->pending && p->next.at_us <= p->now_us) {
        SeqEvent ev = p->next;
        p->pending = 0;
        p->base_us = ev.at_us;
        p->events++;
        emitted++;
        if (p->sink) p->sink(p->user, &ev);
        if (ev.kind == SEQ_EV_TEMPO) {
            p->tempo_us = ev.tempo_us;
        }
        if (ev.kind == SEQ_EV_END) {
            if (!p->looping) {
                p->playing = 0;
                break;
            }
            p->pos = p->track_start;
            p->running = 0;
            if (++guard > SEQ_MAX_EVENTS_PER_ADVANCE) {
                fail(p);
                break;
            }
        }
        fetch(p);
    }
    return emitted;
}
