/* GP0 packet decoder. See gpu_gp0.h for the contract. Plain C99, standard headers only. */
#include "gpu_gp0.h"

#include <string.h>

#define END_NODE 0x00FFFFFFu
#define NODE_MAX_WORDS 256u

static int sx11(uint32_t v) {
    v &= 0x7FFu;
    return (v & 0x400u) ? (int)v - 0x800 : (int)v;
}

static unsigned raw_x(uint32_t v) { return v & 0x3FFu; }
static unsigned raw_y(uint32_t v) { return (v >> 16) & 0x1FFu; }

/* Size fields of copy and transfer commands: 0 means the full VRAM extent. */
static unsigned xsize(uint32_t v) { unsigned x = v & 0x3FFu; return x ? x : 1024u; }
static unsigned ysize(uint32_t v) { unsigned y = (v >> 16) & 0x1FFu; return y ? y : 512u; }

static void set_rgb(GpItem *it, int i, uint32_t c) {
    it->r[i] = (uint8_t)(c & 0xFFu);
    it->g[i] = (uint8_t)((c >> 8) & 0xFFu);
    it->b[i] = (uint8_t)((c >> 16) & 0xFFu);
}

static GpItem *push_item(GpOut *o) {
    GpItem *it;
    if (o->items_n >= o->items_cap) {
        return NULL;
    }
    it = &o->items[o->items_n++];
    memset(it, 0, sizeof *it);
    return it;
}

static void stamp(GpItem *it, const GpState *st) {
    it->off_x = st->off_x;
    it->off_y = st->off_y;
    it->area_x0 = st->area_x0;
    it->area_y0 = st->area_y0;
    it->area_x1 = st->area_x1;
    it->area_y1 = st->area_y1;
    it->tex_window = st->tex_window;
    it->tpage = st->tpage;
    it->semi_mode = (uint8_t)((st->tpage >> 5) & 3u);
    if (st->mask_set) {
        it->flags |= GP_F_MASK_SET;
    }
    if (st->mask_check) {
        it->flags |= GP_F_MASK_CHECK;
    }
    if (st->tpage & 0x0200u) {
        it->flags |= GP_F_DITHER;
    }
}

static void apply_env(GpState *st, uint8_t op, uint32_t w0) {
    switch (op) {
    case 0xE1:
        st->tpage = (uint16_t)(w0 & 0xFFFFu);
        break;
    case 0xE2:
        st->tex_window = w0 & 0xFFFFFu;
        break;
    case 0xE3:   /* bits 0-9 X, bits 10-19 Y (the reference reads ten bits for Y too) */
        st->area_x0 = (uint16_t)(w0 & 0x3FFu);
        st->area_y0 = (uint16_t)((w0 >> 10) & 0x3FFu);
        break;
    case 0xE4:
        st->area_x1 = (uint16_t)(w0 & 0x3FFu);
        st->area_y1 = (uint16_t)((w0 >> 10) & 0x3FFu);
        break;
    case 0xE5:
        st->off_x = (int16_t)sx11(w0);
        st->off_y = (int16_t)sx11(w0 >> 11);
        break;
    case 0xE6:
        st->mask_set = (uint8_t)(w0 & 1u);
        st->mask_check = (uint8_t)((w0 >> 1) & 1u);
        break;
    default:
        break;
    }
}

static int emit_env(GpOut *o, const GpState *st, uint8_t op, uint32_t w0) {
    GpItem *it = push_item(o);
    if (!it) {
        return GP_ERR_FULL;
    }
    it->type = GP_ITEM_ENV;
    it->env_op = op;
    it->env_word = w0 & 0xFFFFFFu;
    stamp(it, st);
    return GP_OK;
}

static int emit_line(GpOut *o, const GpState *st, uint16_t flags,
                     int ax, int ay, uint32_t ca, int bx, int by, uint32_t cb) {
    GpItem *it = push_item(o);
    if (!it) {
        return GP_ERR_FULL;
    }
    it->type = GP_ITEM_LINE;
    it->vert_count = 2;
    it->flags = flags;
    stamp(it, st);
    it->x[0] = (int16_t)(ax + st->off_x);
    it->y[0] = (int16_t)(ay + st->off_y);
    it->x[1] = (int16_t)(bx + st->off_x);
    it->y[1] = (int16_t)(by + st->off_y);
    set_rgb(it, 0, ca);
    set_rgb(it, 1, cb);
    return GP_OK;
}

static int decode_fill(const uint32_t *w, const GpState *st, GpOut *o) {
    GpItem *it = push_item(o);
    if (!it) {
        return GP_ERR_FULL;
    }
    it->type = GP_ITEM_FILL;
    set_rgb(it, 0, w[0]);
    it->x[0] = (int16_t)raw_x(w[1]);
    it->y[0] = (int16_t)raw_y(w[1]);
    it->w = (uint16_t)(w[2] & 0x3FFu);
    it->h = (uint16_t)((w[2] >> 16) & 0x1FFu);
    stamp(it, st);
    return GP_OK;
}

static int decode_poly(const uint32_t *w, uint8_t op, GpState *st, GpOut *o) {
    int gour = (op & 0x10) != 0;
    int quad = (op & 0x08) != 0;
    int tex = (op & 0x04) != 0;
    int semi = (op & 0x02) != 0;
    int raw = (op & 0x01) != 0;
    int nv = quad ? 4 : 3;
    size_t p = 1;
    uint32_t col = w[0];
    GpItem *it;
    int v;

    if (!o->items || o->items_n >= o->items_cap) {
        return GP_ERR_FULL;
    }
    /* The textured polygon's own tpage word is latched into draw state before the state is read. It is the high
     * half of the second UV word: index 4 for flat, 5 for gouraud. */
    if (tex) {
        st->tpage = (uint16_t)((st->tpage & 0xFE00u) | ((w[gour ? 5 : 4] >> 16) & 0x01FFu));
    }
    it = push_item(o);
    it->type = quad ? GP_ITEM_QUAD : GP_ITEM_TRI;
    it->vert_count = (uint8_t)nv;
    it->flags = (uint16_t)((gour ? GP_F_GOURAUD : 0) | (tex ? GP_F_TEXTURED : 0) |
                           (tex && raw ? GP_F_RAW_TEX : 0) | (semi ? GP_F_SEMI : 0));
    stamp(it, st);

    for (v = 0; v < nv; ++v) {
        uint32_t vt;
        if (v > 0 && gour) {
            col = w[p++];
        }
        vt = w[p++];
        it->x[v] = (int16_t)(sx11(vt & 0xFFFFu) + st->off_x);
        it->y[v] = (int16_t)(sx11(vt >> 16) + st->off_y);
        set_rgb(it, v, col);
        if (tex) {
            uint32_t uv = w[p++];
            it->u[v] = (uint8_t)(uv & 0xFFu);
            it->v[v] = (uint8_t)((uv >> 8) & 0xFFu);
            if (v == 0) {
                it->clut = (uint16_t)(uv >> 16);
            }
        }
    }
    return GP_OK;
}

static int decode_line(const uint32_t *w, uint8_t op, const GpState *st, GpOut *o) {
    int gour = (op & 0x10) != 0;
    int semi = (op & 0x02) != 0;
    uint16_t flags = (uint16_t)((gour ? GP_F_GOURAUD : 0) | (semi ? GP_F_SEMI : 0));
    uint32_t c0 = w[0], c1 = w[0];
    int x0, y0, x1, y1;
    if (gour) {
        c1 = w[2];
        x0 = sx11(w[1] & 0xFFFFu);
        y0 = sx11(w[1] >> 16);
        x1 = sx11(w[3] & 0xFFFFu);
        y1 = sx11(w[3] >> 16);
    } else {
        x0 = sx11(w[1] & 0xFFFFu);
        y0 = sx11(w[1] >> 16);
        x1 = sx11(w[2] & 0xFFFFu);
        y1 = sx11(w[2] >> 16);
    }
    return emit_line(o, st, flags, x0, y0, c0, x1, y1, c1);
}

/* Polyline, as the reference's GP0 state machine (runtime/src/gpu.c, gpu_write_gp0_body): the header colour, then vertex
 * words. Mono: the first two vertices are consumed unconditionally, and from the third word on the word is tested for the
 * terminator 5xxx5xxx. Shaded: after the header, V0, then C1, V1, then (C2, V2) pairs; only the colour word of the third
 * and later vertices is tested. A segment is drawn when the next vertex arrives, from the previous vertex and its colour. */
static int decode_polyline(const uint32_t *w, size_t n, uint8_t op, const GpState *st, GpOut *o, size_t *used) {
    int gour = (op & 0x10) != 0;
    int semi = (op & 0x02) != 0;
    uint16_t flags = (uint16_t)(GP_F_POLYLINE | (gour ? GP_F_GOURAUD : 0) | (semi ? GP_F_SEMI : 0));
    uint32_t col = w[0] & 0xFFFFFFu;
    uint32_t prev_col = col;
    int has_prev = 0;
    int px = 0, py = 0;
    int done = 0;
    size_t p = 1;

    while (p < n) {
        uint32_t word = w[p];
        if (!gour) {
            int cx, cy;
            if (has_prev >= 2 && (word & 0xF000F000u) == 0x50005000u) {
                p++;
                done = 1;
                break;
            }
            p++;
            cx = sx11(word & 0xFFFFu);
            cy = sx11(word >> 16);
            if (has_prev) {
                int rc = emit_line(o, st, flags, px, py, col, cx, cy, col);
                if (rc != GP_OK) return rc;
            }
            px = cx;
            py = cy;
            if (has_prev < 2) has_prev++;
            continue;
        }
        if (has_prev == 0) {            /* V0 */
            p++;
            px = sx11(word & 0xFFFFu);
            py = sx11(word >> 16);
            prev_col = col;
            has_prev = 1;
            continue;
        }
        if (has_prev == 1 || has_prev == 3) {   /* colour word; only from the third vertex on can it terminate */
            if (has_prev == 3 && (word & 0xF000F000u) == 0x50005000u) {
                p++;
                done = 1;
                break;
            }
            p++;
            col = word & 0xFFFFFFu;
            has_prev = 2;
            continue;
        }
        {                                /* has_prev == 2: vertex word, never a terminator */
            int cx, cy, rc;
            p++;
            cx = sx11(word & 0xFFFFu);
            cy = sx11(word >> 16);
            rc = emit_line(o, st, flags, px, py, prev_col, cx, cy, col);
            if (rc != GP_OK) return rc;
            px = cx;
            py = cy;
            prev_col = col;
            has_prev = 3;
        }
    }
    *used = p;
    if (!done) {
        *used = n;
        o->truncated++;
    }
    return GP_OK;
}

static int decode_rect(const uint32_t *w, uint8_t op, const GpState *st, GpOut *o) {
    static const uint16_t fixed_size[4] = {0, 1, 8, 16};
    int tex = (op & 0x04) != 0;
    int semi = (op & 0x02) != 0;
    int raw = (op & 0x01) != 0;
    unsigned sz = (op >> 3) & 3u;
    int var = (sz == 0);
    size_t p = 2;
    GpItem *it = push_item(o);
    uint16_t flags;
    if (!it) {
        return GP_ERR_FULL;
    }
    flags = (uint16_t)((tex ? GP_F_TEXTURED : 0) | (tex && raw ? GP_F_RAW_TEX : 0) |
                       (semi ? GP_F_SEMI : 0) | (var ? GP_F_VAR_SIZE : 0));
    it->type = GP_ITEM_RECT;
    it->flags = flags;
    stamp(it, st);

    it->x[0] = (int16_t)(sx11(w[1] & 0xFFFFu) + st->off_x);
    it->y[0] = (int16_t)(sx11(w[1] >> 16) + st->off_y);
    set_rgb(it, 0, w[0]);
    if (tex) {
        uint32_t uv = w[2];
        it->u[0] = (uint8_t)(uv & 0xFFu);
        it->v[0] = (uint8_t)((uv >> 8) & 0xFFu);
        it->clut = (uint16_t)(uv >> 16);
        p = 3;
    }
    if (var) {
        /* Textured rectangles keep 10/9 bits of size, as the reference's gp0_exec_textured_rect does. Monochrome ones keep
         * the whole halfword and the rasteriser clamps to 1023 x 511 (reference gp0_exec_mono_rect). */
        if (tex) {
            it->w = (uint16_t)(w[p] & 0x3FFu);
            it->h = (uint16_t)((w[p] >> 16) & 0x1FFu);
        } else {
            it->w = (uint16_t)(w[p] & 0xFFFFu);
            it->h = (uint16_t)((w[p] >> 16) & 0xFFFFu);
        }
    } else {
        it->w = fixed_size[sz];
        it->h = fixed_size[sz];
    }
    return GP_OK;
}

static int decode_copy_vv(const uint32_t *w, const GpState *st, GpOut *o) {
    GpItem *it = push_item(o);
    if (!it) {
        return GP_ERR_FULL;
    }
    it->type = GP_ITEM_COPY_VRAM;
    it->x[0] = (int16_t)raw_x(w[1]);
    it->y[0] = (int16_t)raw_y(w[1]);
    it->x[1] = (int16_t)raw_x(w[2]);
    it->y[1] = (int16_t)raw_y(w[2]);
    it->w = (uint16_t)xsize(w[3]);
    it->h = (uint16_t)ysize(w[3]);
    stamp(it, st);
    return GP_OK;
}

/* CPU->VRAM: three header words, then (w*h+1)/2 payload words. A payload cut short by the end of the run is kept as
 * far as it goes and marked GP_F_TRUNCATED. */
static int decode_to_vram(const uint32_t *w, size_t n, const GpState *st, GpOut *o, size_t *used) {
    unsigned wd = xsize(w[2]);
    unsigned ht = ysize(w[2]);
    size_t need = ((size_t)wd * (size_t)ht + 1u) / 2u;
    size_t avail = n - 3u;
    size_t take = avail < need ? avail : need;
    int trunc = take < need;
    GpItem *it;

    if (o->items_n >= o->items_cap || o->payload_n + take > o->payload_cap) {
        return GP_ERR_FULL;
    }
    it = push_item(o);
    it->type = GP_ITEM_COPY_TO_VRAM;
    it->x[0] = (int16_t)raw_x(w[1]);
    it->y[0] = (int16_t)raw_y(w[1]);
    it->w = (uint16_t)wd;
    it->h = (uint16_t)ht;
    it->payload_off = (uint32_t)o->payload_n;
    it->payload_n = (uint32_t)take;
    if (take) {
        memcpy(o->payload + o->payload_n, w + 3, take * sizeof(uint32_t));
    }
    o->payload_n += take;
    if (trunc) {
        it->flags |= GP_F_TRUNCATED;
        o->truncated++;
    }
    stamp(it, st);
    *used = 3u + take;
    return GP_OK;
}

static int decode_to_cpu(const uint32_t *w, const GpState *st, GpOut *o) {
    GpItem *it = push_item(o);
    if (!it) {
        return GP_ERR_FULL;
    }
    it->type = GP_ITEM_COPY_TO_CPU;
    it->x[0] = (int16_t)raw_x(w[1]);
    it->y[0] = (int16_t)raw_y(w[1]);
    it->w = (uint16_t)xsize(w[2]);
    it->h = (uint16_t)ysize(w[2]);
    stamp(it, st);
    return GP_OK;
}

static int decode_command(const uint32_t *w, size_t n, GpState *st, GpOut *o, size_t *used) {
    uint8_t op = (uint8_t)(w[0] >> 24);
    int need = gp0_fixed_words(op);

    o->commands++;
    *used = 1;
    if (need == 0) {
        o->unknown++;
        return GP_OK;
    }
    if (need < 0) {
        return decode_polyline(w, n, op, st, o, used);
    }
    if ((size_t)need > n) {
        *used = n;
        o->truncated++;
        return GP_OK;
    }
    *used = (size_t)need;

    if (op == 0x02) {
        return decode_fill(w, st, o);
    }
    if (op >= 0x20 && op <= 0x3F) {
        return decode_poly(w, op, st, o);
    }
    if (op >= 0x40 && op <= 0x5F) {
        return decode_line(w, op, st, o);
    }
    if (op >= 0x60 && op <= 0x7F) {
        return decode_rect(w, op, st, o);
    }
    if (op >= 0x80 && op <= 0x9F) {
        return decode_copy_vv(w, st, o);
    }
    if (op >= 0xA0 && op <= 0xBF) {
        return decode_to_vram(w, n, st, o, used);
    }
    if (op >= 0xC0 && op <= 0xDF) {
        return decode_to_cpu(w, st, o);
    }
    if (op >= 0xE1 && op <= 0xE6) {
        apply_env(st, op, w[0]);
        return emit_env(o, st, op, w[0]);
    }
    return GP_OK;   /* nops: 0x00, 0x01, 0x03..0x1F, 0xE0, 0xE7..0xEF, 0xFF */
}

void gp0_state_init(GpState *st) {
    memset(st, 0, sizeof *st);
    st->area_x1 = 1023;
    st->area_y1 = 511;
}

void gp0_out_init(GpOut *o, GpItem *items, size_t items_cap, uint32_t *payload, size_t payload_cap) {
    memset(o, 0, sizeof *o);
    o->items = items;
    o->items_cap = items_cap;
    o->payload = payload;
    o->payload_cap = payload_cap;
}

int gp0_fixed_words(uint8_t op) {
    unsigned nv, tex;
    if (op == 0x02) {
        return 3;
    }
    if (op <= 0x1F) {
        return 1;   /* 0x00, 0x01, 0x03..0x1E nops; 0x1F IRQ request */
    }
    if (op <= 0x3F) {
        nv = (op & 0x08) ? 4u : 3u;
        tex = (op & 0x04) ? 2u : 1u;
        return (int)(1u + nv * tex + ((op & 0x10) ? nv - 1u : 0u));
    }
    if (op <= 0x5F) {
        if (op & 0x08) {
            return -1;   /* polyline */
        }
        return (op & 0x10) ? 4 : 3;
    }
    if (op <= 0x7F) {
        int tex = (op & 0x04) ? 1 : 0;
        int var = ((op >> 3) & 3) == 0;
        return 2 + tex + var;
    }
    if (op <= 0x9F) {
        return 4;   /* VRAM -> VRAM copy */
    }
    if (op <= 0xDF) {
        return 3;   /* CPU -> VRAM and VRAM -> CPU headers */
    }
    if (op <= 0xEF) {
        return 1;   /* 0xE0..0xEF: E1..E6 environment, the rest nops */
    }
    if (op == 0xFF) {
        return 1;   /* nop, as gpu.c treats it */
    }
    return 0;       /* 0xF0..0xFE: undefined in the reference table */
}

int gp0_decode_words(const uint32_t *words, size_t n, GpState *st, GpOut *o) {
    size_t i = 0;
    if (!st || !o || (!words && n)) {
        return GP_ERR_ARG;
    }
    while (i < n) {
        size_t used = 1;
        int rc = decode_command(words + i, n - i, st, o, &used);
        if (rc != GP_OK) {
            return rc;
        }
        i += used;
    }
    return GP_OK;
}

int gp0_walk_ordering(GpReadWordFn read, void *ctx, uint32_t first, unsigned max_nodes,
                      GpState *st, GpOut *o) {
    uint32_t addr = first;
    uint32_t buf[NODE_MAX_WORDS];
    unsigned k;
    if (!read || !st || !o) {
        return GP_ERR_ARG;
    }
    if ((first & 0xFFFFFFu) == END_NODE) {
        return GP_OK;
    }
    for (k = 0; k < max_nodes; ++k) {
        uint32_t hdr = read(ctx, addr);
        uint32_t next = hdr & 0xFFFFFFu;
        unsigned count = (unsigned)(hdr >> 24);
        unsigned i;
        o->nodes++;
        for (i = 0; i < count; ++i) {
            buf[i] = read(ctx, addr + 4u + 4u * i);
        }
        if (count) {
            int rc = gp0_decode_words(buf, count, st, o);
            if (rc != GP_OK) {
                return rc;
            }
        }
        if (next == END_NODE) {
            return GP_OK;
        }
        addr = next & 0xFFFFFCu;
    }
    return GP_ERR_NODES;
}
