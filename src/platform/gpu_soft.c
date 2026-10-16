/* CPU software rasterizer for decoded GP0 draw items. See gpu_soft.h.
 *
 * Port of JoJoPS1/psxrecomp/runtime/src/gpu_sw_renderer.c (native scale-1 path) and of the command rules in
 * runtime/src/gpu.c (gp0_exec_*). Float expressions keep the reference's operand order, because the interpolated colours
 * and texture coordinates are truncated to integers and any reordering can move a texel by one. Do not "simplify" them. */
#include "gpu_soft.h"

#define SOFT_W GPU_VRAM_W
#define SOFT_H GPU_VRAM_H

/* Per-item drawing state. Filled from the item once; every primitive routine reads only this. */
typedef struct {
    uint16_t *buf;              /* SOFT_W x SOFT_H, row-major */
    int cx1, cy1, cx2, cy2;     /* drawing area, inclusive, clamped to VRAM */
    int semi_en, semi_mode;     /* semi-transparency enabled and mode 0..3 */
    int mask_set, mask_check;   /* E6 bits */
    int tw_mx, tw_my, tw_ox, tw_oy;   /* texture window: mask and offset, 5 bits each, in 8-pixel steps */
    int mod_r, mod_g, mod_b;    /* colour modulation, 5-bit (vertex colour >> 3) */
    int raw;                    /* raw texture: no modulation */
    uint32_t *prov;             /* optional provenance array (see GpuSoftConfig) */
    uint32_t prov_id;
} Soft;

static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }
static int iabs(int v) { return v < 0 ? -v : v; }

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint16_t vram_get(const Soft *s, int x, int y) {
    return s->buf[(y & (SOFT_H - 1)) * SOFT_W + (x & (SOFT_W - 1))];
}

/* ---- Semi-transparency: the reference's blend_pixels, same operand order and clamps. ---- */
static uint16_t blend_pixels(uint16_t back, uint16_t front, int mode) {
    int br = (back >> 0) & 0x1F, bg = (back >> 5) & 0x1F, bb = (back >> 10) & 0x1F;
    int fr = (front >> 0) & 0x1F, fg = (front >> 5) & 0x1F, fb = (front >> 10) & 0x1F;
    int r, g, b;
    switch (mode) {
    case 0:
        r = (br + fr) >> 1;
        g = (bg + fg) >> 1;
        b = (bb + fb) >> 1;
        break;
    case 1:
        r = br + fr; if (r > 31) r = 31;
        g = bg + fg; if (g > 31) g = 31;
        b = bb + fb; if (b > 31) b = 31;
        break;
    case 2:
        r = br - fr; if (r < 0) r = 0;
        g = bg - fg; if (g < 0) g = 0;
        b = bb - fb; if (b < 0) b = 0;
        break;
    case 3:
        r = br + (fr >> 2); if (r > 31) r = 31;
        g = bg + (fg >> 2); if (g > 31) g = 31;
        b = bb + (fb >> 2); if (b > 31) b = 31;
        break;
    default:
        r = fr; g = fg; b = fb;
        break;
    }
    return (uint16_t)(r | (g << 5) | (b << 10));
}

/* ---- Pixel writes: the reference's put_opaque and put_textured. ---- */
static void put_opaque(const Soft *s, int x, int y, uint16_t color) {
    int idx;
    if (x < 0 || x >= SOFT_W || y < 0 || y >= SOFT_H) return;
    if (x < s->cx1 || x > s->cx2 || y < s->cy1 || y > s->cy2) return;
    idx = y * SOFT_W + x;
    if (s->mask_check && (s->buf[idx] & 0x8000)) return;
    if (s->semi_en) color = blend_pixels(s->buf[idx], color, s->semi_mode);
    if (s->mask_set) color |= 0x8000;
    s->buf[idx] = color;
    if (s->prov) s->prov[idx] = s->prov_id;
}

static void put_textured(const Soft *s, int x, int y, uint16_t texel, int mr, int mg, int mb, int raw) {
    int idx;
    uint16_t color;
    if (x < 0 || x >= SOFT_W || y < 0 || y >= SOFT_H) return;
    if (x < s->cx1 || x > s->cx2 || y < s->cy1 || y > s->cy2) return;
    if (texel == 0x0000) return;           /* transparent texel: never drawn */
    idx = y * SOFT_W + x;
    if (s->mask_check && (s->buf[idx] & 0x8000)) return;
    if (!raw) {
        int tr = (texel >> 0) & 0x1F, tg = (texel >> 5) & 0x1F, tb = (texel >> 10) & 0x1F;
        int r = (tr * mr) >> 4; if (r > 31) r = 31;
        int g = (tg * mg) >> 4; if (g > 31) g = 31;
        int b = (tb * mb) >> 4; if (b > 31) b = 31;
        color = (uint16_t)(r | (g << 5) | (b << 10));
    } else {
        color = (uint16_t)(texel & 0x7FFF);
    }
    if (s->semi_en && (texel & 0x8000)) color = blend_pixels(s->buf[idx], color, s->semi_mode);
    if (s->mask_set) color |= 0x8000;
    s->buf[idx] = color;
    if (s->prov) s->prov[idx] = s->prov_id;
}

/* ---- Texture lookup: the reference's texel_fetch (texture window, 4/8/15-bit, CLUT). ---- */
static uint16_t texel_fetch(const Soft *s, int u, int v, uint16_t texpage, int clut_x, int clut_y) {
    int tpx, tpy, depth;
    if (s->tw_mx | s->tw_my) {
        u = (u & ~(s->tw_mx * 8)) | ((s->tw_ox & s->tw_mx) * 8);
        v = (v & ~(s->tw_my * 8)) | ((s->tw_oy & s->tw_my) * 8);
    }
    tpx = (texpage & 0xF) * 64;
    tpy = ((texpage >> 4) & 1) * 256;
    depth = (texpage >> 7) & 3;
    switch (depth) {
    case 0: {
        uint16_t word = vram_get(s, tpx + (u / 4), tpy + v);
        int index = (word >> ((u & 3) * 4)) & 0xF;
        return vram_get(s, clut_x + index, clut_y);
    }
    case 1: {
        uint16_t word = vram_get(s, tpx + (u / 2), tpy + v);
        int index = (word >> ((u & 1) * 8)) & 0xFF;
        return vram_get(s, clut_x + index, clut_y);
    }
    default:
        return vram_get(s, tpx + u, tpy + v);
    }
}

/* ---- Triangle coverage: the psx 32.32 biased DDA with exclusive bottom and right edges (gpu_sw_edges.h). ---- */
static int64_t edge_fp(int x) {
    return ((int64_t)x << 32) + ((1LL << 32) - (1 << 11));
}

static int64_t edge_step(int dx, int dy) {
    int64_t rounding = dx < 0 ? -(int64_t)(dy - 1) : (dx > 0 ? (int64_t)(dy - 1) : 0);
    return (((int64_t)dx << 32) + rounding) / dy;
}

static int edge_unfp(int64_t fp) {
    return (int)((uint64_t)fp >> 32);
}

/* Vertices must already be sorted by y. Returns the long edge (x0..x2) and the short edge (x0..x1 or x1..x2) at scanline y. */
static void tri_edges_at_y(int x0, int y0, int x1, int y1, int x2, int y2, int y, int *long_x, int *short_x) {
    int64_t long_step = edge_step(x2 - x0, y2 - y0);
    *long_x = edge_unfp(edge_fp(x0) + long_step * (y - y0));
    if (y < y1) {
        int64_t short_step = edge_step(x1 - x0, y1 - y0);
        *short_x = edge_unfp(edge_fp(x0) + short_step * (y - y0));
    } else {
        int64_t short_step = edge_step(x2 - x1, y2 - y1);
        *short_x = edge_unfp(edge_fp(x1) + short_step * (y - y1));
    }
}

/* ---- Flat triangle: the reference's raster_flat_triangle. ---- */
static void tri_flat(const Soft *s, int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
    int tt, y;
    if (y0 > y1) { tt = x0; x0 = x1; x1 = tt; tt = y0; y0 = y1; y1 = tt; }
    if (y0 > y2) { tt = x0; x0 = x2; x2 = tt; tt = y0; y0 = y2; y2 = tt; }
    if (y1 > y2) { tt = x1; x1 = x2; x2 = tt; tt = y1; y1 = y2; y2 = tt; }
    if (y2 - y0 == 0) return;
    for (y = y0; y < y2; y++) {
        int xa, xb, sx, ex, x;
        if (y < s->cy1 || y > s->cy2) continue;
        tri_edges_at_y(x0, y0, x1, y1, x2, y2, y, &xa, &xb);
        if (xa > xb) { tt = xa; xa = xb; xb = tt; }
        sx = imax(xa, s->cx1);
        ex = imin(xb, s->cx2 + 1);
        for (x = sx; x < ex; x++) put_opaque(s, x, y, color);
    }
}

/* ---- Gouraud triangle: the reference's raster_gouraud_triangle (colours in 5-bit space). ---- */
typedef struct { int x, y, r, g, b; } GVert;

static void tri_gouraud(const Soft *s, GVert a, GVert b, GVert c) {
    int dy_total, y, tt;
    int x0 = a.x, y0 = a.y, r0 = a.r, g0 = a.g, b0 = a.b;
    int x1 = b.x, y1 = b.y, r1 = b.r, g1 = b.g, b1 = b.b;
    int x2 = c.x, y2 = c.y, r2 = c.r, g2 = c.g, b2 = c.b;

    if (y0 > y1) {
        tt = x0; x0 = x1; x1 = tt; tt = y0; y0 = y1; y1 = tt;
        tt = r0; r0 = r1; r1 = tt; tt = g0; g0 = g1; g1 = tt; tt = b0; b0 = b1; b1 = tt;
    }
    if (y0 > y2) {
        tt = x0; x0 = x2; x2 = tt; tt = y0; y0 = y2; y2 = tt;
        tt = r0; r0 = r2; r2 = tt; tt = g0; g0 = g2; g2 = tt; tt = b0; b0 = b2; b2 = tt;
    }
    if (y1 > y2) {
        tt = x1; x1 = x2; x2 = tt; tt = y1; y1 = y2; y2 = tt;
        tt = r1; r1 = r2; r2 = tt; tt = g1; g1 = g2; g2 = tt; tt = b1; b1 = b2; b2 = tt;
    }

    dy_total = y2 - y0;
    if (dy_total == 0) return;

    for (y = y0; y < y2; y++) {
        int second_half, seg_height, xa, xb, ra, ga, ba, rb, gb, bb, sx, ex, span, x;
        float alpha, beta;
        if (y < s->cy1 || y > s->cy2) continue;

        second_half = (y >= y1);
        seg_height = second_half ? (y2 - y1) : (y1 - y0);
        if (seg_height == 0) seg_height = 1;

        alpha = (float)(y - y0) / (float)dy_total;
        if (second_half)
            beta = (float)(y - y1) / (float)seg_height;
        else
            beta = (float)(y - y0) / (float)seg_height;

        tri_edges_at_y(x0, y0, x1, y1, x2, y2, y, &xa, &xb);

        ra = r0 + (int)((float)(r2 - r0) * alpha);
        ga = g0 + (int)((float)(g2 - g0) * alpha);
        ba = b0 + (int)((float)(b2 - b0) * alpha);
        if (second_half) {
            rb = r1 + (int)((float)(r2 - r1) * beta);
            gb = g1 + (int)((float)(g2 - g1) * beta);
            bb = b1 + (int)((float)(b2 - b1) * beta);
        } else {
            rb = r0 + (int)((float)(r1 - r0) * beta);
            gb = g0 + (int)((float)(g1 - g0) * beta);
            bb = b0 + (int)((float)(b1 - b0) * beta);
        }

        if (xa > xb) {
            tt = xa; xa = xb; xb = tt;
            tt = ra; ra = rb; rb = tt;
            tt = ga; ga = gb; gb = tt;
            tt = ba; ba = bb; bb = tt;
        }

        sx = imax(xa, s->cx1);
        ex = imin(xb, s->cx2 + 1);
        span = xb - xa;

        for (x = sx; x < ex; x++) {
            uint16_t color;
            if (span > 0) {
                float tf = (float)(x - xa) / (float)span;
                int r = ra + (int)((float)(rb - ra) * tf);
                int g = ga + (int)((float)(gb - ga) * tf);
                int b = ba + (int)((float)(bb - ba) * tf);
                if (r < 0) r = 0;
                if (r > 31) r = 31;
                if (g < 0) g = 0;
                if (g > 31) g = 31;
                if (b < 0) b = 0;
                if (b > 31) b = 31;
                color = (uint16_t)(r | (g << 5) | (b << 10));
            } else {
                color = (uint16_t)(ra | (ga << 5) | (ba << 10));
            }
            put_opaque(s, x, y, color);
        }
    }
}

/* ---- Textured triangle: the reference's raster_textured_triangle (no perspective, no filter). ---- */
typedef struct { int x, y, u, v, r, g, b; } TVert;

static void tri_textured(const Soft *s, TVert a, TVert b, TVert c, uint16_t clut_x, uint16_t clut_y, uint16_t texpage,
                         int shaded, int raw) {
    int dy_total, y, tt;
    int x0 = a.x, y0 = a.y, u0 = a.u, v0 = a.v, r0 = a.r, g0 = a.g, b0 = a.b;
    int x1 = b.x, y1 = b.y, u1 = b.u, v1 = b.v, r1 = b.r, g1 = b.g, b1 = b.b;
    int x2 = c.x, y2 = c.y, u2 = c.u, v2 = c.v, r2 = c.r, g2 = c.g, b2 = c.b;

    if (y0 > y1) {
        tt = x0; x0 = x1; x1 = tt; tt = y0; y0 = y1; y1 = tt;
        tt = u0; u0 = u1; u1 = tt; tt = v0; v0 = v1; v1 = tt;
        tt = r0; r0 = r1; r1 = tt; tt = g0; g0 = g1; g1 = tt; tt = b0; b0 = b1; b1 = tt;
    }
    if (y0 > y2) {
        tt = x0; x0 = x2; x2 = tt; tt = y0; y0 = y2; y2 = tt;
        tt = u0; u0 = u2; u2 = tt; tt = v0; v0 = v2; v2 = tt;
        tt = r0; r0 = r2; r2 = tt; tt = g0; g0 = g2; g2 = tt; tt = b0; b0 = b2; b2 = tt;
    }
    if (y1 > y2) {
        tt = x1; x1 = x2; x2 = tt; tt = y1; y1 = y2; y2 = tt;
        tt = u1; u1 = u2; u2 = tt; tt = v1; v1 = v2; v2 = tt;
        tt = r1; r1 = r2; r2 = tt; tt = g1; g1 = g2; g2 = tt; tt = b1; b1 = b2; b2 = tt;
    }

    dy_total = y2 - y0;
    if (dy_total == 0) return;

    for (y = y0; y < y2; y++) {
        int second_half, seg_height, xa, xb, span, sx, ex, x;
        float alpha, beta, ua, va, ub, vb, ra = 0.0f, ga = 0.0f, ba = 0.0f, rb = 0.0f, gb = 0.0f, bb = 0.0f;
        if (y < s->cy1 || y > s->cy2) continue;

        second_half = (y >= y1);
        seg_height = second_half ? (y2 - y1) : (y1 - y0);
        if (seg_height == 0) seg_height = 1;

        alpha = (float)(y - y0) / (float)dy_total;
        if (second_half)
            beta = (float)(y - y1) / (float)seg_height;
        else
            beta = (float)(y - y0) / (float)seg_height;

        tri_edges_at_y(x0, y0, x1, y1, x2, y2, y, &xa, &xb);

        ua = u0 + (float)(u2 - u0) * alpha;
        va = v0 + (float)(v2 - v0) * alpha;
        if (shaded) {
            ra = r0 + (float)(r2 - r0) * alpha;
            ga = g0 + (float)(g2 - g0) * alpha;
            ba = b0 + (float)(b2 - b0) * alpha;
        }
        if (second_half) {
            ub = u1 + (float)(u2 - u1) * beta;
            vb = v1 + (float)(v2 - v1) * beta;
            if (shaded) {
                rb = r1 + (float)(r2 - r1) * beta;
                gb = g1 + (float)(g2 - g1) * beta;
                bb = b1 + (float)(b2 - b1) * beta;
            }
        } else {
            ub = u0 + (float)(u1 - u0) * beta;
            vb = v0 + (float)(v1 - v0) * beta;
            if (shaded) {
                rb = r0 + (float)(r1 - r0) * beta;
                gb = g0 + (float)(g1 - g0) * beta;
                bb = b0 + (float)(b1 - b0) * beta;
            }
        }

        if (xa > xb) {
            float tf;
            tt = xa; xa = xb; xb = tt;
            tf = ua; ua = ub; ub = tf;
            tf = va; va = vb; vb = tf;
            if (shaded) {
                tf = ra; ra = rb; rb = tf;
                tf = ga; ga = gb; gb = tf;
                tf = ba; ba = bb; bb = tf;
            }
        }

        span = xb - xa;
        if (span == 0) span = 1;

        sx = imax(xa, s->cx1);
        ex = imin(xb, s->cx2 + 1);

        for (x = sx; x < ex; x++) {
            float t_val = (float)(x - xa) / (float)span;
            float fu = ua + (ub - ua) * t_val;
            float fv = va + (vb - va) * t_val;
            uint16_t texel = texel_fetch(s, (int)fu & 0xFF, (int)fv & 0xFF, texpage, clut_x, clut_y);
            if (shaded) {
                int mr = (int)(ra + (rb - ra) * t_val);
                int mg = (int)(ga + (gb - ga) * t_val);
                int mb = (int)(ba + (bb - ba) * t_val);
                if (mr < 0) mr = 0;
                if (mr > 31) mr = 31;
                if (mg < 0) mg = 0;
                if (mg > 31) mg = 31;
                if (mb < 0) mb = 0;
                if (mb > 31) mb = 31;
                put_textured(s, x, y, texel, mr, mg, mb, raw);
            } else {
                put_textured(s, x, y, texel, s->mod_r, s->mod_g, s->mod_b, raw);
            }
        }
    }
}

/* ---- Rectangles: the reference's raster_flat_rect and raster_textured_rect (scale 1). ---- */
static void rect_flat(const Soft *s, int x, int y, int w, int h, uint16_t color) {
    int row;
    for (row = 0; row < h; row++) {
        int py = y + row, px, sx, ex;
        if (py < s->cy1 || py > s->cy2) continue;
        sx = imax(x, s->cx1);
        ex = imin(x + w - 1, s->cx2);
        for (px = sx; px <= ex; px++) put_opaque(s, px, py, color);
    }
}

static void rect_textured(const Soft *s, int x, int y, int w, int h, int u, int v, uint16_t clut_x, uint16_t clut_y,
                          uint16_t texpage) {
    int row;
    for (row = 0; row < h; row++) {
        int py = y + row, col, tv;
        if (py < s->cy1 || py > s->cy2) continue;
        tv = (v + row) & 0xFF;
        for (col = 0; col < w; col++) {
            int px = x + col, tu;
            uint16_t texel;
            if (px < s->cx1 || px > s->cx2) continue;
            tu = (u + col) & 0xFF;
            texel = texel_fetch(s, tu, tv, texpage, clut_x, clut_y);
            put_textured(s, px, py, texel, s->mod_r, s->mod_g, s->mod_b, s->raw);
        }
    }
}

/* Textured rectangle whose texel footprint is stretched from (u0,v0)-(u1,v1) over the destination (reference
 * raster_textured_rect_scaled, scale 1). */
static void rect_textured_scaled(const Soft *s, int x, int y, int w, int h, int u0, int v0, int u1, int v1,
                                 uint16_t clut_x, uint16_t clut_y, uint16_t texpage) {
    int du, dv, row;
    if (w <= 0 || h <= 0) return;
    du = u1 - u0;
    dv = v1 - v0;
    for (row = 0; row < h; row++) {
        int py = y + row, col, tv;
        if (py < s->cy1 || py > s->cy2) continue;
        tv = (int)(v0 + ((int64_t)dv * row) / h) & 0xFF;
        for (col = 0; col < w; col++) {
            int px = x + col, tu;
            uint16_t texel;
            if (px < s->cx1 || px > s->cx2) continue;
            tu = (int)(u0 + ((int64_t)du * col) / w) & 0xFF;
            texel = texel_fetch(s, tu, tv, texpage, clut_x, clut_y);
            put_textured(s, px, py, texel, s->mod_r, s->mod_g, s->mod_b, s->raw);
        }
    }
}

/* ---- Lines: the reference's sw_draw_line (Bresenham) and sw_draw_shaded_line. ---- */
static void line_flat(const Soft *s, int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = iabs(x1 - x0);
    int dy = -iabs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        put_opaque(s, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
}

static void line_shaded(const Soft *s, int x0, int y0, uint16_t c0, int x1, int y1, uint16_t c1) {
    int dx = iabs(x1 - x0);
    int dy = -iabs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    int total = imax(iabs(x1 - x0), iabs(y1 - y0));
    int r0 = c0 & 0x1F, g0 = (c0 >> 5) & 0x1F, b0 = (c0 >> 10) & 0x1F;
    int r1 = c1 & 0x1F, g1 = (c1 >> 5) & 0x1F, b1 = (c1 >> 10) & 0x1F;
    int step = 0;
    if (total == 0) total = 1;
    for (;;) {
        int r = r0 + (r1 - r0) * step / total;
        int g = g0 + (g1 - g0) * step / total;
        int b = b0 + (b1 - b0) * step / total;
        put_opaque(s, x0, y0, (uint16_t)(r | (g << 5) | (b << 10)));
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
        step++;
    }
}

/* ---- Command rules (gpu.c): rejection, quads, item dispatch. ---- */

/* psx_gpu_triangle_oversize: a triangle whose x span exceeds 1023 or whose y span exceeds 511 is not drawn. */
static int tri_oversize(const int *x, const int *y, int a, int b, int c) {
    int minx = x[a], maxx = x[a], miny = y[a], maxy = y[a];
    if (x[b] < minx) minx = x[b];
    if (x[b] > maxx) maxx = x[b];
    if (x[c] < minx) minx = x[c];
    if (x[c] > maxx) maxx = x[c];
    if (maxx - minx > 1023) return 1;
    if (y[b] < miny) miny = y[b];
    if (y[b] > maxy) maxy = y[b];
    if (y[c] < miny) miny = y[c];
    if (y[c] > maxy) maxy = y[c];
    return maxy - miny > 511;
}

/* psx_gpu_line_oversize */
static int line_oversize(int x0, int y0, int x1, int y1) {
    int dx = x0 > x1 ? x0 - x1 : x1 - x0;
    int dy = y0 > y1 ? y0 - y1 : y1 - y0;
    return dx > 1023 || dy > 511;
}

/* ws_axis_aligned_quad: the four vertices are exactly the corners of their bounding box. */
static int axis_aligned_quad(const int *x, const int *y) {
    int minx = x[0], maxx = x[0], miny = y[0], maxy = y[0], i;
    unsigned corners = 0;
    for (i = 1; i < 4; i++) {
        if (x[i] < minx) minx = x[i];
        if (x[i] > maxx) maxx = x[i];
        if (y[i] < miny) miny = y[i];
        if (y[i] > maxy) maxy = y[i];
    }
    for (i = 0; i < 4; i++) {
        if (x[i] == minx && y[i] == miny) corners |= 1u;
        else if (x[i] == maxx && y[i] == miny) corners |= 2u;
        else if (x[i] == minx && y[i] == maxy) corners |= 4u;
        else if (x[i] == maxx && y[i] == maxy) corners |= 8u;
        else return 0;
    }
    return corners == 15u;
}

static uint16_t rgb555_of(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10));
}

static void tri_item(const Soft *s, const GpItem *it) {
    int x[3], y[3], i;
    for (i = 0; i < 3; i++) { x[i] = it->x[i]; y[i] = it->y[i]; }
    if (tri_oversize(x, y, 0, 1, 2)) return;
    if (!(it->flags & GP_F_TEXTURED)) {
        if (it->flags & GP_F_GOURAUD) {
            GVert a = { x[0], y[0], it->r[0] >> 3, it->g[0] >> 3, it->b[0] >> 3 };
            GVert b = { x[1], y[1], it->r[1] >> 3, it->g[1] >> 3, it->b[1] >> 3 };
            GVert c = { x[2], y[2], it->r[2] >> 3, it->g[2] >> 3, it->b[2] >> 3 };
            tri_gouraud(s, a, b, c);
        } else {
            tri_flat(s, x[0], y[0], x[1], y[1], x[2], y[2], rgb555_of(it->r[0], it->g[0], it->b[0]));
        }
        return;
    } else {
        int raw = (it->flags & GP_F_RAW_TEX) ? 1 : 0;
        int shaded = (it->flags & GP_F_GOURAUD) ? 1 : 0;
        TVert a = { x[0], y[0], it->u[0], it->v[0], it->r[0] >> 3, it->g[0] >> 3, it->b[0] >> 3 };
        TVert b = { x[1], y[1], it->u[1], it->v[1], it->r[1] >> 3, it->g[1] >> 3, it->b[1] >> 3 };
        TVert c = { x[2], y[2], it->u[2], it->v[2], it->r[2] >> 3, it->g[2] >> 3, it->b[2] >> 3 };
        tri_textured(s, a, b, c, (uint16_t)((it->clut & 0x3F) * 16), (uint16_t)((it->clut >> 6) & 0x1FF),
                     (uint16_t)(it->tpage & 0x1FF), shaded, raw);
    }
}

/* Mono quad (reference gp0_exec_mono_quad): full-screen rectangle, semi axis-aligned rectangle, else two flat triangles. */
static void quad_flat(const Soft *s, const GpItem *it, int disp_w, int disp_h) {
    int x[4], y[4], i, rej_a, rej_b;
    uint16_t color = rgb555_of(it->r[0], it->g[0], it->b[0]);
    for (i = 0; i < 4; i++) { x[i] = it->x[i]; y[i] = it->y[i]; }
    rej_a = tri_oversize(x, y, 0, 1, 2);
    rej_b = tri_oversize(x, y, 2, 1, 3);
    if (rej_a && rej_b) return;

    {
        /* Full-screen filter: an axis-aligned quad covering the whole display, measured before the draw offset. */
        int ox = it->off_x, oy = it->off_y;
        int rx0 = x[0] - ox, rx1 = x[1] - ox, rx2 = x[2] - ox, rx3 = x[3] - ox;
        int ry0 = y[0] - oy, ry1 = y[1] - oy, ry2 = y[2] - oy, ry3 = y[3] - oy;
        if (rx0 == rx2 && rx1 == rx3 && ry0 == ry1 && ry2 == ry3 &&
            rx1 > rx0 && ry2 > ry0 && rx0 <= 0 && rx1 >= disp_w && ry0 <= 0 && ry2 >= disp_h) {
            rect_flat(s, x[0], y[0], x[1] - x[0], y[2] - y[0], color);
            return;
        }
    }

    if ((it->flags & GP_F_SEMI) && axis_aligned_quad(x, y)) {
        int minx = x[0], maxx = x[0], miny = y[0], maxy = y[0];
        for (i = 1; i < 4; i++) {
            if (x[i] < minx) minx = x[i];
            if (x[i] > maxx) maxx = x[i];
            if (y[i] < miny) miny = y[i];
            if (y[i] > maxy) maxy = y[i];
        }
        if (maxx - minx > 0 && maxy - miny > 0) {
            rect_flat(s, minx, miny, maxx - minx, maxy - miny, color);
            return;
        }
    }

    if (!rej_a) tri_flat(s, x[0], y[0], x[1], y[1], x[2], y[2], color);
    if (!rej_b) tri_flat(s, x[2], y[2], x[1], y[1], x[3], y[3], color);
}

static void quad_gouraud(const Soft *s, const GpItem *it) {
    int x[4], y[4], i, rej_a, rej_b;
    GVert v[4];
    for (i = 0; i < 4; i++) {
        x[i] = it->x[i]; y[i] = it->y[i];
        v[i].x = x[i]; v[i].y = y[i];
        v[i].r = it->r[i] >> 3; v[i].g = it->g[i] >> 3; v[i].b = it->b[i] >> 3;
    }
    rej_a = tri_oversize(x, y, 0, 1, 2);
    rej_b = tri_oversize(x, y, 2, 1, 3);
    if (rej_a && rej_b) return;
    if (!rej_a) tri_gouraud(s, v[0], v[1], v[2]);
    if (!rej_b) tri_gouraud(s, v[2], v[1], v[3]);
}

/* Textured quad (reference gp0_exec_textured_quad). An axis-aligned quad with one uv rectangle is a textured rectangle. */
static void quad_textured(const Soft *s, const GpItem *it, int shaded, int raw) {
    int x[4], y[4], u[4], v[4], i, rej_a, rej_b;
    uint16_t clut_x = (uint16_t)((it->clut & 0x3F) * 16);
    uint16_t clut_y = (uint16_t)((it->clut >> 6) & 0x1FF);
    uint16_t tpage = (uint16_t)(it->tpage & 0x1FF);
    GVert cv[4];
    for (i = 0; i < 4; i++) {
        x[i] = it->x[i]; y[i] = it->y[i];
        u[i] = it->u[i]; v[i] = it->v[i];
        cv[i].x = x[i]; cv[i].y = y[i];
        cv[i].r = it->r[i] >> 3; cv[i].g = it->g[i] >> 3; cv[i].b = it->b[i] >> 3;
    }
    rej_a = tri_oversize(x, y, 0, 1, 2);
    rej_b = tri_oversize(x, y, 2, 1, 3);
    if (rej_a && rej_b) return;

    if (!shaded) {
        int as_rect = y[0] == y[1] && y[2] == y[3] && x[0] == x[2] && x[1] == x[3] &&
                      u[0] == u[2] && u[1] == u[3] && v[0] == v[1] && v[2] == v[3];
        if (as_rect) {
            int rx = x[0] < x[1] ? x[0] : x[1];
            int ry = y[0] < y[2] ? y[0] : y[2];
            int w = x[0] < x[1] ? x[1] - x[0] : x[0] - x[1];
            int h = y[0] < y[2] ? y[2] - y[0] : y[0] - y[2];
            int left_u = x[0] < x[1] ? u[0] : u[1];
            int right_u = x[0] < x[1] ? u[1] : u[0];
            int top_v = y[0] < y[2] ? v[0] : v[2];
            int bot_v = y[0] < y[2] ? v[2] : v[0];
            if (w > 0 && h > 0) {
                if (right_u - left_u == w && bot_v - top_v == h)
                    rect_textured(s, rx, ry, w, h, left_u, top_v, clut_x, clut_y, tpage);
                else
                    rect_textured_scaled(s, rx, ry, w, h, left_u, top_v, right_u, bot_v, clut_x, clut_y, tpage);
                return;
            }
        }
        if (!rej_a) {
            GVert a = cv[0], b = cv[1], c = cv[2];
            TVert ta = { a.x, a.y, u[0], v[0], a.r, a.g, a.b };
            TVert tb = { b.x, b.y, u[1], v[1], b.r, b.g, b.b };
            TVert tc = { c.x, c.y, u[2], v[2], c.r, c.g, c.b };
            tri_textured(s, ta, tb, tc, clut_x, clut_y, tpage, 0, raw);
        }
        if (!rej_b) {
            TVert ta = { x[2], y[2], u[2], v[2], cv[2].r, cv[2].g, cv[2].b };
            TVert tb = { x[1], y[1], u[1], v[1], cv[1].r, cv[1].g, cv[1].b };
            TVert tc = { x[3], y[3], u[3], v[3], cv[3].r, cv[3].g, cv[3].b };
            tri_textured(s, ta, tb, tc, clut_x, clut_y, tpage, 0, raw);
        }
        return;
    }

    /* Shaded textured quad: two shaded triangles, no rectangle rule. */
    if (!rej_a) {
        TVert ta = { x[0], y[0], u[0], v[0], cv[0].r, cv[0].g, cv[0].b };
        TVert tb = { x[1], y[1], u[1], v[1], cv[1].r, cv[1].g, cv[1].b };
        TVert tc = { x[2], y[2], u[2], v[2], cv[2].r, cv[2].g, cv[2].b };
        tri_textured(s, ta, tb, tc, clut_x, clut_y, tpage, 1, raw);
    }
    if (!rej_b) {
        TVert ta = { x[2], y[2], u[2], v[2], cv[2].r, cv[2].g, cv[2].b };
        TVert tb = { x[1], y[1], u[1], v[1], cv[1].r, cv[1].g, cv[1].b };
        TVert tc = { x[3], y[3], u[3], v[3], cv[3].r, cv[3].g, cv[3].b };
        tri_textured(s, ta, tb, tc, clut_x, clut_y, tpage, 1, raw);
    }
}

static void quad_item(const Soft *s, const GpItem *it, const GpuSoftConfig *cfg) {
    int tex = (it->flags & GP_F_TEXTURED) != 0;
    int gour = (it->flags & GP_F_GOURAUD) != 0;
    int raw = (it->flags & GP_F_RAW_TEX) != 0;
    if (!tex && !gour) quad_flat(s, it, cfg->disp_w, cfg->disp_h);
    else if (!tex) quad_gouraud(s, it);
    else quad_textured(s, it, gour, raw);
}

static void line_item(const Soft *s, const GpItem *it) {
    if (line_oversize(it->x[0], it->y[0], it->x[1], it->y[1])) return;
    if (it->flags & GP_F_GOURAUD)
        line_shaded(s, it->x[0], it->y[0], rgb555_of(it->r[0], it->g[0], it->b[0]),
                    it->x[1], it->y[1], rgb555_of(it->r[1], it->g[1], it->b[1]));
    else
        line_flat(s, it->x[0], it->y[0], it->x[1], it->y[1], rgb555_of(it->r[0], it->g[0], it->b[0]));
}

static void rect_item(const Soft *s, const GpItem *it) {
    int w = it->w, h = it->h;
    if (w > 1023) w = 1023;
    if (h > 511) h = 511;
    if (w <= 0 || h <= 0) return;
    if (it->flags & GP_F_TEXTURED) {
        rect_textured(s, it->x[0], it->y[0], w, h, it->u[0], it->v[0],
                      (uint16_t)((it->clut & 0x3F) * 16), (uint16_t)((it->clut >> 6) & 0x1FF),
                      (uint16_t)(it->tpage & 0x1FF));
    } else {
        rect_flat(s, it->x[0], it->y[0], w, h, rgb555_of(it->r[0], it->g[0], it->b[0]));
    }
}

void gpu_soft_draw(GpuVram *vram, const GpItem *it, const GpuSoftConfig *cfg) {
    static const GpuSoftConfig k_default = { 320, 240, NULL, 0 };
    Soft s;
    GpuSoftConfig c;
    if (!vram || !it) return;
    c = cfg ? *cfg : k_default;
    if (c.disp_w <= 0) c.disp_w = 320;
    if (c.disp_h <= 0) c.disp_h = 240;

    s.buf = &vram->px[0][0];
    s.cx1 = clampi((int)it->area_x0, 0, SOFT_W - 1);
    s.cy1 = clampi((int)it->area_y0, 0, SOFT_H - 1);
    s.cx2 = clampi((int)it->area_x1, 0, SOFT_W - 1);
    s.cy2 = clampi((int)it->area_y1, 0, SOFT_H - 1);
    s.semi_en = (it->flags & GP_F_SEMI) ? 1 : 0;
    s.semi_mode = it->semi_mode & 3;
    s.mask_set = (it->flags & GP_F_MASK_SET) ? 1 : 0;
    s.mask_check = (it->flags & GP_F_MASK_CHECK) ? 1 : 0;
    s.tw_mx = (int)(it->tex_window & 0x1Fu);
    s.tw_my = (int)((it->tex_window >> 5) & 0x1Fu);
    s.tw_ox = (int)((it->tex_window >> 10) & 0x1Fu);
    s.tw_oy = (int)((it->tex_window >> 15) & 0x1Fu);
    s.mod_r = it->r[0] >> 3;
    s.mod_g = it->g[0] >> 3;
    s.mod_b = it->b[0] >> 3;
    s.raw = (it->flags & GP_F_RAW_TEX) ? 1 : 0;
    s.prov = c.prov;
    s.prov_id = c.prov_id;

    switch (it->type) {
    case GP_ITEM_TRI:
        tri_item(&s, it);
        break;
    case GP_ITEM_QUAD:
        quad_item(&s, it, &c);
        break;
    case GP_ITEM_LINE:
        line_item(&s, it);
        break;
    case GP_ITEM_RECT:
        rect_item(&s, it);
        break;
    default:
        break;
    }
}
