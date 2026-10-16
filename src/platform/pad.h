/* Platform pad module (final-target code, plain C99, no PS1 types, no guest addresses).
 * The shell (GameController) or a test harness sets the controller state; game-facing code reads it once per frame.
 * Button word convention is the game's: bit set = released (active low). Bit 0 select, 1 L3, 2 R3, 3 start, 4 up, 5 right, 6 down,
 * 7 left, 8 L2, 9 R2, 10 L1, 11 R1, 12 triangle, 13 circle, 14 cross, 15 square. */
#ifndef PLAT_PAD_H
#define PLAT_PAD_H
#include <stdint.h>
#include <stddef.h>

#define PLAT_PAD_PORTS 2

typedef struct {
    uint16_t buttons;                 /* active low, see above */
    uint8_t  lx, ly, rx, ry;          /* 0..255, 0x80 = centre (used when analog != 0) */
    uint8_t  connected;               /* 0: nothing plugged in */
    uint8_t  analog;                  /* 0: digital pad, 1: analog pad */
} PlatPad;

void     plat_pad_set(int port, const PlatPad *state);
int      plat_pad_get(int port, PlatPad *out);          /* 0 when the port is out of range */
uint16_t plat_pad_buttons(int port);                    /* 0xFFFF when disconnected */

/* The controller report as the game's pad code reads it (4 bytes digital, 8 bytes analog):
 *   out[0] = 0 when a pad answered (0xFF when none), out[1] = id byte (0x41 digital, 0x73 analog),
 *   out[2] = low byte of the button word, out[3] = high byte, out[4..7] = right x, right y, left x, left y (analog only).
 * Returns the number of bytes written (0 if cap is too small). */
size_t   plat_pad_report(int port, uint8_t *out, size_t cap);
#endif
