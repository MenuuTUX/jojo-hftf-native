/* Bounded, thread-safe GPU frame handoff. The producer applies VRAM operations to the CPU VRAM model and records every
 * draw and VRAM operation in one ordered stream (see gpu_raster.h). Swift receives immutable copies through
 * gpu_raster_copy_published; the replay consumer uses gpu_raster_copy_published_ordered and gpu_raster_replay. */
#include "gpu_raster.h"
#include "gpu_soft.h"

#include <pthread.h>
#include <string.h>

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static GpuVram g_vram;
static GpuVram g_published_vram;
static GpItem g_items[GPU_RASTER_MAX_ITEMS];
static GpItem g_published_items[GPU_RASTER_MAX_ITEMS];
static uint32_t g_payload[GPU_RASTER_MAX_PAYLOAD_WORDS];
static uint32_t g_published_payload[GPU_RASTER_MAX_PAYLOAD_WORDS];
static size_t g_item_count;
static size_t g_payload_count;
static size_t g_dropped;
static size_t g_published_count;
static size_t g_published_payload_count;
static size_t g_published_dropped;
static GpuDisplayEnv g_display = { 0, 0, 384, 240, 1, 0, 0, 0 };
static GpuDisplayEnv g_published_display = { 0, 0, 384, 240, 1, 0, 0, 0 };
static uint64_t g_sequence;
static int g_initialized;
static int g_soft_on = 1;
static GpuSoftConfig g_soft_cfg = { 384, 240, NULL, 0 };
static uint32_t g_prov[GPU_RASTER_VRAM_PIXELS];   /* provenance of the current frame: stream entry index + 1 per written pixel */
static int g_prov_on;

static void ensure_initialized(void) {
    if (!g_initialized) {
        gpu_vram_init(&g_vram);
        gpu_vram_init(&g_published_vram);
        g_initialized = 1;
    }
}

static int is_draw(uint8_t type) {
    return type == GP_ITEM_TRI || type == GP_ITEM_QUAD || type == GP_ITEM_LINE || type == GP_ITEM_RECT;
}

static int is_vram_op(uint8_t type) {
    return type == GP_ITEM_FILL || type == GP_ITEM_COPY_VRAM || type == GP_ITEM_COPY_TO_VRAM;
}

static unsigned mask_of_item(const GpItem *it) {
    return ((it->flags & GP_F_MASK_SET) ? GPU_MASK_SET : 0u) |
           ((it->flags & GP_F_MASK_CHECK) ? GPU_MASK_CHECK : 0u);
}

static uint16_t flags_of_mask(unsigned m) {
    return (uint16_t)(((m & GPU_MASK_SET) ? GP_F_MASK_SET : 0u) | ((m & GPU_MASK_CHECK) ? GP_F_MASK_CHECK : 0u));
}

static size_t transfer_words(const GpItem *it) {
    return ((size_t)it->w * (size_t)it->h + 1u) / 2u;
}

/* Applies one VRAM entry to v. For GP_ITEM_COPY_TO_VRAM, words holds the entry's own pixels (payload_n of them).
 * Returns 0, -1 (missing pixels, or the temporary buffer of a copy could not be allocated), or -2 (not a VRAM entry). */
static int apply_vram(GpuVram *v, const GpItem *it, const uint32_t *words, size_t nwords) {
    switch (it->type) {
    case GP_ITEM_FILL:
        gpu_vram_fill(v, it->x[0], it->y[0], it->w, it->h, gpu_vram_color15(it->r[0], it->g[0], it->b[0]));
        return 0;
    case GP_ITEM_COPY_VRAM:
        return gpu_vram_copy(v, it->x[0], it->y[0], it->x[1], it->y[1], it->w, it->h, mask_of_item(it)) == 0 ? 0 : -1;
    case GP_ITEM_COPY_TO_VRAM:
        if (!words || nwords < transfer_words(it)) return -1;
        gpu_vram_load_words(v, it->x[0], it->y[0], it->w, it->h, words, mask_of_item(it));
        return 0;
    default:
        return -2;
    }
}

/* Provenance of VRAM operations: marks the written rectangle with the entry's id (no-op unless provenance is on). */
static void prov_mark_rect(int x, int y, int w, int h, uint32_t id) {
    int i, j;
    if (!g_prov_on || w <= 0 || h <= 0) return;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            g_prov[((y + j) & (GPU_VRAM_H - 1)) * GPU_VRAM_W + ((x + i) & (GPU_VRAM_W - 1))] = id;
}

/* Appends one entry to the stream. The caller has checked capacity or accepts the drop. words are the entry's own
 * pixels for COPY_TO_VRAM (nwords == payload_n). The entry's payload_off is rebased into the stream payload. */
static void push_locked(const GpItem *it, const uint32_t *words, size_t nwords) {
    GpItem e;
    if (g_item_count >= GPU_RASTER_MAX_ITEMS || g_payload_count + nwords > GPU_RASTER_MAX_PAYLOAD_WORDS) {
        ++g_dropped;
        return;
    }
    e = *it;
    if (it->type == GP_ITEM_COPY_TO_VRAM) {
        e.payload_off = (uint32_t)g_payload_count;
        e.payload_n = (uint32_t)nwords;
        if (nwords) memcpy(g_payload + g_payload_count, words, nwords * sizeof *words);
        g_payload_count += nwords;
    } else {
        e.payload_off = 0;
        e.payload_n = 0;
    }
    g_items[g_item_count++] = e;
}

/* Checks a batch without touching any state: entry kinds, payload bounds and room in the stream. */
static int check_batch(const GpItem *items, size_t count, const uint32_t *payload, size_t payload_count,
                       size_t *need_items, size_t *need_words) {
    size_t i;
    *need_items = 0;
    *need_words = 0;
    for (i = 0; i < count; ++i) {
        const GpItem *it = &items[i];
        switch (it->type) {
        case GP_ITEM_TRI:
        case GP_ITEM_QUAD:
        case GP_ITEM_LINE:
        case GP_ITEM_RECT:
        case GP_ITEM_FILL:
        case GP_ITEM_COPY_VRAM:
            ++*need_items;
            break;
        case GP_ITEM_COPY_TO_VRAM:
            if ((it->flags & GP_F_TRUNCATED) || it->payload_n < transfer_words(it)) return -1;
            if (!payload || it->payload_off > payload_count || it->payload_n > payload_count - it->payload_off) return -1;
            ++*need_items;
            *need_words += it->payload_n;
            break;
        case GP_ITEM_ENV:
        case GP_ITEM_COPY_TO_CPU:
            break;
        default:
            return -1;
        }
    }
    if (*need_items > GPU_RASTER_MAX_ITEMS - g_item_count) return -1;
    if (*need_words > GPU_RASTER_MAX_PAYLOAD_WORDS - g_payload_count) return -1;
    return 0;
}

void gpu_raster_reset(void) {
    pthread_mutex_lock(&g_lock);
    gpu_vram_init(&g_vram);
    gpu_vram_init(&g_published_vram);
    g_item_count = 0;
    g_payload_count = 0;
    g_dropped = 0;
    g_published_count = 0;
    g_published_payload_count = 0;
    g_published_dropped = 0;
    g_sequence = 0;
    g_display = (GpuDisplayEnv){ 0, 0, 384, 240, 1, 0, 0, 0 };
    g_published_display = g_display;
    g_initialized = 1;
    pthread_mutex_unlock(&g_lock);
}

void gpu_raster_set_display(const GpuDisplayEnv *display) {
    if (!display) return;
    pthread_mutex_lock(&g_lock);
    ensure_initialized();
    g_display = *display;
    g_soft_cfg.disp_w = display->w;
    g_soft_cfg.disp_h = display->h;
    pthread_mutex_unlock(&g_lock);
}

void gpu_raster_set_prov(int on) {
    pthread_mutex_lock(&g_lock);
    g_prov_on = on ? 1 : 0;
    if (!g_prov_on) memset(g_prov, 0, sizeof g_prov);
    pthread_mutex_unlock(&g_lock);
}

int gpu_raster_copy_prov(uint32_t *prov, size_t capacity) {
    int ok = 0;
    pthread_mutex_lock(&g_lock);
    if (prov && capacity >= GPU_RASTER_VRAM_PIXELS) {
        memcpy(prov, g_prov, sizeof g_prov);
        ok = 1;
    }
    pthread_mutex_unlock(&g_lock);
    return ok;
}

size_t gpu_raster_copy_stream(GpItem *items, size_t capacity) {
    size_t n;
    pthread_mutex_lock(&g_lock);
    n = g_item_count;
    if (items && capacity >= n && n) memcpy(items, g_items, n * sizeof *items);
    pthread_mutex_unlock(&g_lock);
    return n;
}

void gpu_raster_set_soft(int on) {
    pthread_mutex_lock(&g_lock);
    g_soft_on = on ? 1 : 0;
    pthread_mutex_unlock(&g_lock);
}

int gpu_raster_record(const GpItem *items, size_t count, const uint32_t *payload, size_t payload_count) {
    size_t i, need_items, need_words;
    int rc = 0;
    if (!items && count) return -1;
    pthread_mutex_lock(&g_lock);
    ensure_initialized();
    if (check_batch(items, count, payload, payload_count, &need_items, &need_words) != 0) {
        rc = -1;
    } else {
        for (i = 0; i < count && rc == 0; ++i) {
            const GpItem *it = &items[i];
            if (is_draw(it->type)) {
                g_soft_cfg.prov = g_prov_on ? g_prov : NULL;
                g_soft_cfg.prov_id = (uint32_t)g_item_count + 1u;
                if (g_soft_on) gpu_soft_draw(&g_vram, it, &g_soft_cfg);
                push_locked(it, NULL, 0);
            } else if (is_vram_op(it->type)) {
                const uint32_t *words = (it->type == GP_ITEM_COPY_TO_VRAM) ? payload + it->payload_off : NULL;
                size_t nwords = (it->type == GP_ITEM_COPY_TO_VRAM) ? it->payload_n : 0;
                if (apply_vram(&g_vram, it, words, nwords) != 0) { rc = -1; break; }
                prov_mark_rect(it->x[it->type == GP_ITEM_COPY_VRAM ? 1 : 0], it->y[it->type == GP_ITEM_COPY_VRAM ? 1 : 0],
                               it->w, it->h, (uint32_t)g_item_count + 1u);
                push_locked(it, words, nwords);
            }
            /* GP_ITEM_ENV and GP_ITEM_COPY_TO_CPU are not part of the stream. */
        }
    }
    pthread_mutex_unlock(&g_lock);
    return rc;
}

int gpu_raster_append(const GpItem *items, size_t count) {
    return gpu_raster_record(items, count, NULL, 0);
}

void gpu_raster_load_words(int x, int y, int w, int h, const uint32_t *words, unsigned mask_flags) {
    GpItem it;
    size_t n;
    if (w <= 0 || h <= 0 || w > GPU_VRAM_W || h > GPU_VRAM_H) return;
    memset(&it, 0, sizeof it);
    it.type = GP_ITEM_COPY_TO_VRAM;
    it.x[0] = (int16_t)x; it.y[0] = (int16_t)y;
    it.w = (uint16_t)w; it.h = (uint16_t)h;
    it.flags = flags_of_mask(mask_flags);
    n = transfer_words(&it);
    it.payload_n = (uint32_t)n;
    pthread_mutex_lock(&g_lock);
    ensure_initialized();
    if (words && n) {
        apply_vram(&g_vram, &it, words, n);
        prov_mark_rect(x, y, w, h, (uint32_t)g_item_count + 1u);
        push_locked(&it, words, n);
    }
    pthread_mutex_unlock(&g_lock);
}

void gpu_raster_store(int x, int y, int w, int h, uint16_t *pixels) {
    pthread_mutex_lock(&g_lock);
    ensure_initialized();
    gpu_vram_store(&g_vram, x, y, w, h, pixels);
    pthread_mutex_unlock(&g_lock);
}

int gpu_raster_copy(int sx, int sy, int dx, int dy, int w, int h, unsigned mask_flags) {
    GpItem it;
    int rc;
    if (w <= 0 || h <= 0 || w > GPU_VRAM_W || h > GPU_VRAM_H) return 0;
    memset(&it, 0, sizeof it);
    it.type = GP_ITEM_COPY_VRAM;
    it.x[0] = (int16_t)sx; it.y[0] = (int16_t)sy;
    it.x[1] = (int16_t)dx; it.y[1] = (int16_t)dy;
    it.w = (uint16_t)w; it.h = (uint16_t)h;
    it.flags = flags_of_mask(mask_flags);
    pthread_mutex_lock(&g_lock);
    ensure_initialized();
    rc = apply_vram(&g_vram, &it, NULL, 0);
    if (rc == 0) {
        prov_mark_rect(dx, dy, w, h, (uint32_t)g_item_count + 1u);
        push_locked(&it, NULL, 0);
    }
    pthread_mutex_unlock(&g_lock);
    return rc == 0 ? 0 : -1;
}

void gpu_raster_fill(int x, int y, int w, int h, uint16_t color15) {
    GpItem it;
    if (w <= 0 || h <= 0) return;
    memset(&it, 0, sizeof it);
    it.type = GP_ITEM_FILL;
    it.x[0] = (int16_t)x; it.y[0] = (int16_t)y;
    it.w = (uint16_t)w; it.h = (uint16_t)h;
    /* Stored as 8-bit components whose top five bits are the 15-bit colour, so apply_vram reproduces color15 exactly. */
    it.r[0] = (uint8_t)((color15 & 31u) << 3);
    it.g[0] = (uint8_t)(((color15 >> 5) & 31u) << 3);
    it.b[0] = (uint8_t)(((color15 >> 10) & 31u) << 3);
    pthread_mutex_lock(&g_lock);
    ensure_initialized();
    apply_vram(&g_vram, &it, NULL, 0);
    prov_mark_rect(x, y, w, h, (uint32_t)g_item_count + 1u);
    push_locked(&it, NULL, 0);
    pthread_mutex_unlock(&g_lock);
}

void gpu_raster_publish_frame(void) {
    pthread_mutex_lock(&g_lock);
    ensure_initialized();
    memcpy(g_published_items, g_items, g_item_count * sizeof *g_items);
    memcpy(g_published_payload, g_payload, g_payload_count * sizeof *g_payload);
    g_published_count = g_item_count;
    g_published_payload_count = g_payload_count;
    g_published_dropped = g_dropped;
    g_item_count = 0;
    g_payload_count = 0;
    g_dropped = 0;
    memcpy(&g_published_vram, &g_vram, sizeof g_vram);
    g_published_display = g_display;
    if (g_prov_on) memset(g_prov, 0, sizeof g_prov);
    ++g_sequence;
    pthread_mutex_unlock(&g_lock);
}

int gpu_raster_copy_published(GpItem *items, size_t item_capacity, size_t *item_count,
                              uint16_t *vram, size_t vram_capacity, GpuDisplayEnv *display,
                              uint64_t *sequence) {
    int ok = 0;
    pthread_mutex_lock(&g_lock);
    ensure_initialized();
    if (item_count) *item_count = g_published_count;
    if (sequence) *sequence = g_sequence;
    if (display) *display = g_published_display;
    if ((!items && g_published_count) || item_capacity < g_published_count ||
        (!vram && vram_capacity) || vram_capacity < GPU_RASTER_VRAM_PIXELS) {
        /* refused: nothing copied */
    } else {
        if (g_published_count) memcpy(items, g_published_items, g_published_count * sizeof *items);
        memcpy(vram, g_published_vram.px, GPU_RASTER_VRAM_PIXELS * sizeof *vram);
        ok = 1;
    }
    pthread_mutex_unlock(&g_lock);
    return ok;
}

int gpu_raster_copy_published_ordered(GpItem *items, size_t item_capacity, size_t *item_count,
                                      uint32_t *payload, size_t payload_capacity, size_t *payload_count,
                                      uint16_t *vram, size_t vram_capacity,
                                      GpuDisplayEnv *display, uint64_t *sequence) {
    int ok = 0;
    pthread_mutex_lock(&g_lock);
    ensure_initialized();
    if (item_count) *item_count = g_published_count;
    if (payload_count) *payload_count = g_published_payload_count;
    if (sequence) *sequence = g_sequence;
    if (display) *display = g_published_display;
    if ((!items && g_published_count) || item_capacity < g_published_count ||
        (!payload && g_published_payload_count) || payload_capacity < g_published_payload_count ||
        (!vram && vram_capacity) || vram_capacity < GPU_RASTER_VRAM_PIXELS) {
        /* refused: nothing copied */
    } else {
        if (g_published_count) memcpy(items, g_published_items, g_published_count * sizeof *items);
        if (g_published_payload_count) memcpy(payload, g_published_payload, g_published_payload_count * sizeof *payload);
        memcpy(vram, g_published_vram.px, GPU_RASTER_VRAM_PIXELS * sizeof *vram);
        ok = g_published_dropped ? -1 : 1;
    }
    pthread_mutex_unlock(&g_lock);
    return ok;
}

int gpu_raster_copy_live_vram(uint16_t *vram, size_t vram_capacity) {
    int ok = 0;
    pthread_mutex_lock(&g_lock);
    ensure_initialized();
    if (vram && vram_capacity >= GPU_RASTER_VRAM_PIXELS) {
        memcpy(vram, g_vram.px, GPU_RASTER_VRAM_PIXELS * sizeof *vram);
        ok = 1;
    }
    pthread_mutex_unlock(&g_lock);
    return ok;
}

int gpu_raster_replay(GpuVram *vram, const GpuSoftConfig *cfg, const GpItem *items, size_t count,
                      const uint32_t *payload, size_t payload_count,
                      GpuReplayVisitFn visit, void *ctx) {
    size_t i;
    GpuSoftConfig c = { 384, 240, NULL, 0 };
    if (cfg) c = *cfg;
    if (!vram || (!items && count)) return -1;
    for (i = 0; i < count; ++i) {
        const GpItem *it = &items[i];
        int rc;
        if (is_draw(it->type)) {
            if (visit) visit(ctx, it, vram);
            gpu_soft_draw(vram, it, &c);
        } else if (is_vram_op(it->type)) {
            const uint32_t *words = NULL;
            size_t nwords = 0;
            if (it->type == GP_ITEM_COPY_TO_VRAM) {
                if (!payload || it->payload_off > payload_count || it->payload_n > payload_count - it->payload_off) return -1;
                words = payload + it->payload_off;
                nwords = it->payload_n;
            }
            rc = apply_vram(vram, it, words, nwords);
            if (rc != 0) return rc;
        } else {
            return -2;
        }
    }
    return 0;
}
