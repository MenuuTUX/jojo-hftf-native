/* In-process host glue for the Swift shell (development stage only). Header and threading rules: hybrid/jojo_host_shell.h.
 *
 * Link-level wrapping, no edits to the adapters: the JOJO_SHELL_LIB block of JoJoPS1/CMakeLists.txt compiles
 *   hybrid/jojo_pad.c   with  jojo_pad_frame  renamed to  jojo_pad_frame_impl
 *   hybrid/jojo_audio.c with  jojo_audio_frame and jojo_audio_pull  renamed to  *_impl
 * so the strong definitions below take their place. Every other caller keeps calling the same names.
 *
 * Per vblank (game thread, from main.cpp through jojo_pad_frame), in this order:
 *   1. the host controller word is ANDED into the SIO model for each port that has one (active low: a pressed button clears
 *      its bit, released bits leave the runtime's value alone, so the debug server's route and press still apply);
 *   2. jojo_pad_frame_impl samples SIO and publishes the delayed report (unchanged adapter);
 *   3. pacing: sleep to the 60 Hz deadline, stats, and autoquit.
 * Audio: the game thread pushes one 735-frame tick (jojo_audio_frame) under g_audio_lock; the render thread pulls with
 * a try-lock, so it never waits on the game. Both sides are counted. */
#include "jojo_host_shell.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Adapter entry points as renamed by the JOJO_SHELL_LIB block (the originals live in hybrid/jojo_pad.c and jojo_audio.c). */
extern void jojo_pad_frame_impl(void);
extern void jojo_audio_frame_impl(void);
extern size_t jojo_audio_pull_impl(int16_t *out, size_t frames);
/* Runtime entry: main.cpp's main, renamed by the JOJO_SHELL_LIB block. The C++ mangled name is spelled out here. */
extern int psxrecomp_main(int argc, char **argv) __asm__("__Z14psxrecomp_mainiPPc");
/* Runtime and frame services (SIO model in sio.c, frame counter in jojo_core.c). */
extern uint16_t sio_get_pad_buttons_slot(int slot);
extern void     sio_set_pad_state_slot(int slot, uint16_t buttons);
extern uint64_t jojo_frame_now(void);

#define HOST_PERIOD_NS   16666667ull          /* 60 Hz: one game tick */
#define HOST_RESYNC_NS   100000000ull         /* a vblank this late resets the pacing origin instead of bursting */

static pthread_mutex_t g_audio_lock = PTHREAD_MUTEX_INITIALIZER;

/* Controller state from the shell: bit 31 valid, bit 16 connected, bits 0..15 the PS1 word (active low). */
static uint32_t g_pad[2];

static int      g_pace = 1;
static uint64_t g_quit_at;

/* Audio counters: written from both threads with atomic adds. */
static uint64_t g_a_pull_calls, g_a_under_calls, g_a_under_frames, g_a_contended, g_a_frames_pulled, g_a_ticks;

/* Vblank counters and pacing: game thread only. */
static int      g_have_origin;
static uint64_t g_origin_ns, g_origin_frame;
static uint64_t g_prev_ns;
static double   g_interval_sum_ms, g_interval_max_ms;
static uint64_t g_intervals, g_vblanks, g_late, g_last_frame;

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void sleep_ns(uint64_t ns) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ns / 1000000000ull);
    ts.tv_nsec = (long)(ns % 1000000000ull);
    while (nanosleep(&ts, &ts) != 0) { }
}

static uint64_t env_u64(const char *name, uint64_t dflt) {
    const char *e = getenv(name);
    if (!e || !*e) return dflt;
    return (uint64_t)strtoull(e, NULL, 10);
}

static void print_stats(const char *why) {
    JojoHostStats s;
    jojo_host_stats(&s);
    fprintf(stderr,
            "jojo-host: %s frame=%llu vblanks=%llu vblank_ms mean=%.3f max=%.3f late=%llu "
            "audio ticks=%llu pulls=%llu pulled=%llu underflow_calls=%llu underflow_frames=%llu contended=%llu\n",
            why, (unsigned long long)s.frame, (unsigned long long)s.vblanks, s.vblank_ms_mean, s.vblank_ms_max,
            (unsigned long long)s.late_vblanks, (unsigned long long)s.audio_ticks, (unsigned long long)s.audio_pull_calls,
            (unsigned long long)s.audio_frames_pulled, (unsigned long long)s.audio_underflow_calls,
            (unsigned long long)s.audio_underflow_frames, (unsigned long long)s.audio_contended_calls);
    fflush(stderr);
}

/* Runs the runtime. The environment is read here, before the runtime starts. */
int jojo_host_run(int argc, char **argv) {
    g_pace = env_u64("JOJO_SHELL_PACE", 1) != 0;
    g_quit_at = env_u64("JOJO_SHELL_AUTOQUIT_FRAMES", 0);
    fprintf(stderr, "jojo-host: starting runtime pace=%d autoquit_frames=%llu\n", g_pace, (unsigned long long)g_quit_at);
    fflush(stderr);
    return psxrecomp_main(argc, argv);
}

void jojo_host_set_pad(int port, uint16_t buttons, int connected) {
    if (port < 0 || port > 1) return;
    uint32_t v = 0x80000000u | (connected ? 0x10000u : 0u) | (uint32_t)buttons;
    __atomic_store_n(&g_pad[port], v, __ATOMIC_RELAXED);
}

/* Vblank hook for pacing, stats and autoquit. Game thread. */
static void vblank(uint64_t frame) {
    uint64_t now = now_ns();
    if (!g_have_origin) {
        g_have_origin = 1;
        g_origin_ns = now;
        g_origin_frame = frame;
    }
    if (g_pace) {
        uint64_t deadline = g_origin_ns + (frame - g_origin_frame) * HOST_PERIOD_NS;
        if (now < deadline) {
            sleep_ns(deadline - now);
            now = now_ns();
        } else {
            g_late++;
            if (now > deadline + HOST_RESYNC_NS) g_origin_ns = now - (frame - g_origin_frame) * HOST_PERIOD_NS;
        }
    }
    if (g_prev_ns) {
        double ms = (double)(now - g_prev_ns) / 1e6;
        g_interval_sum_ms += ms;
        if (ms > g_interval_max_ms) g_interval_max_ms = ms;
        g_intervals++;
    }
    g_prev_ns = now;
    g_vblanks++;
    g_last_frame = frame;
    if (g_quit_at && frame >= g_quit_at) {
        print_stats("autoquit");
        exit(0);
    }
}

/* Vblank entry (replaces the adapter's jojo_pad_frame, see the file comment). */
void jojo_pad_frame(void) {
    for (int port = 0; port < 2; port++) {
        uint32_t v = __atomic_load_n(&g_pad[port], __ATOMIC_RELAXED);
        if (!(v & 0x80000000u)) continue;
        uint16_t host = (uint16_t)(v & 0xFFFFu);
        uint16_t runtime = sio_get_pad_buttons_slot(port);
        sio_set_pad_state_slot(port, (uint16_t)(runtime & host));
    }
    jojo_pad_frame_impl();
    vblank(jojo_frame_now());
}

/* Game-thread tick (replaces the adapter's jojo_audio_frame). Serialised against the render thread. */
void jojo_audio_frame(void) {
    pthread_mutex_lock(&g_audio_lock);
    jojo_audio_frame_impl();
    pthread_mutex_unlock(&g_audio_lock);
    __atomic_fetch_add(&g_a_ticks, 1, __ATOMIC_RELAXED);
}

/* Render-thread pull (replaces the adapter's jojo_audio_pull). Never waits: on contention it zero-fills and reports it. */
size_t jojo_audio_pull(int16_t *out, size_t frames) {
    __atomic_fetch_add(&g_a_pull_calls, 1, __ATOMIC_RELAXED);
    if (pthread_mutex_trylock(&g_audio_lock) != 0) {
        memset(out, 0, frames * 2 * sizeof(int16_t));
        __atomic_fetch_add(&g_a_contended, 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(&g_a_under_calls, 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(&g_a_under_frames, frames, __ATOMIC_RELAXED);
        return 0;
    }
    size_t n = jojo_audio_pull_impl(out, frames);
    pthread_mutex_unlock(&g_audio_lock);
    __atomic_fetch_add(&g_a_frames_pulled, n, __ATOMIC_RELAXED);
    if (n < frames) {
        __atomic_fetch_add(&g_a_under_calls, 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(&g_a_under_frames, frames - n, __ATOMIC_RELAXED);
    }
    return n;
}

void jojo_host_stats(JojoHostStats *out) {
    if (!out) return;
    memset(out, 0, sizeof *out);
    out->audio_pull_calls = __atomic_load_n(&g_a_pull_calls, __ATOMIC_RELAXED);
    out->audio_underflow_calls = __atomic_load_n(&g_a_under_calls, __ATOMIC_RELAXED);
    out->audio_underflow_frames = __atomic_load_n(&g_a_under_frames, __ATOMIC_RELAXED);
    out->audio_contended_calls = __atomic_load_n(&g_a_contended, __ATOMIC_RELAXED);
    out->audio_frames_pulled = __atomic_load_n(&g_a_frames_pulled, __ATOMIC_RELAXED);
    out->audio_ticks = __atomic_load_n(&g_a_ticks, __ATOMIC_RELAXED);
    out->vblanks = g_vblanks;
    out->late_vblanks = g_late;
    out->vblank_ms_mean = g_intervals ? g_interval_sum_ms / (double)g_intervals : 0.0;
    out->vblank_ms_max = g_interval_max_ms;
    out->frame = g_last_frame;
}
