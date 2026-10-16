/* In-process host for the Swift shell (development stage only, see shell/README.md "Hosted runtime").
 *
 * The shell links the psx-runtime object files (JOJO_SHELL_LIB=ON build, see the last block of JoJoPS1/CMakeLists.txt).
 * The runtime's main() is renamed to psxrecomp_main there. jojo_host_run() calls it on a thread the shell creates, so
 * the game runs headless (no SDL window, no SDL audio) and the shell presents its published GPU frames, plays its audio
 * ring and feeds its controller state.
 *
 * Threads:
 *   game thread   jojo_host_run and everything under it. The vblank hooks (jojo_pad_frame, jojo_audio_frame) run here.
 *   main thread   AppKit, Metal presentation, GameController polling (jojo_host_set_pad).
 *   audio thread  CoreAudio render callback (jojo_audio_pull). Never blocks: it try-locks and zero-fills on contention.
 *
 * Environment read once by jojo_host_run:
 *   JOJO_SHELL_PACE=0|1                 pace the game to 60 vblanks per second (default 1). 0 runs as fast as possible.
 *   JOJO_SHELL_AUTOQUIT_FRAMES=<n>      exit(0) from the game thread at the vblank of frame n (after the stats line). 0 = off.
 * Everything else (JOJO_PHASE1, JOJO_FILE_LAYER, JOJO_EXTRACTED_DIR, JOJO_PAD_*) is read by the pieces themselves. */
#ifndef JOJO_HOST_SHELL_H
#define JOJO_HOST_SHELL_H

#include <stddef.h>
#include <stdint.h>

/* Runs the runtime on the calling thread with the usual runtime arguments (for example --headless --debug-port 4430
 * --memcard-dir <dir>). Returns the runtime's status; the runtime may also exit(0) itself (TCP quit, autoquit). */
int jojo_host_run(int argc, char **argv);

/* Controller state for the next vblanks. buttons is the PS1 word, active low (0xFFFF = all released), as platform/pad.h
 * defines it. The host ANDs it with what the runtime already put into the SIO model for that port, so the debug server's
 * input route and press commands still work while the controller is idle. Call from any thread. port 0 or 1. */
void jojo_host_set_pad(int port, uint16_t buttons, int connected);

/* Audio pull for the CoreAudio render callback: up to `frames` 44.1 kHz stereo int16 frames (interleaved L,R) from the
 * native audio ring. Returns the frames that came from the ring; the rest of out is zero-filled. Thread safe with the game
 * thread. Never blocks for more than a few microseconds. */
size_t jojo_audio_pull(int16_t *out, size_t frames);

typedef struct {
    uint64_t audio_pull_calls;      /* calls from the render thread */
    uint64_t audio_underflow_calls; /* calls that got fewer frames than asked (zero-filled) */
    uint64_t audio_underflow_frames;/* frames zero-filled because the ring was short */
    uint64_t audio_contended_calls; /* calls that found the producer holding the lock (all zero-filled) */
    uint64_t audio_frames_pulled;   /* frames returned from the ring */
    uint64_t audio_ticks;           /* game ticks that pushed audio (jojo_audio_frame calls) */
    uint64_t vblanks;               /* pad/vblank hook calls on the game thread */
    uint64_t late_vblanks;          /* vblanks whose pacing deadline had already passed (pacing on) */
    double   vblank_ms_mean;        /* mean interval between vblank hook calls on the game thread, ms */
    double   vblank_ms_max;         /* largest such interval, ms */
    uint64_t frame;                 /* jojo_frame_now() at the last vblank */
} JojoHostStats;

void jojo_host_stats(JojoHostStats *out);

#endif
