/* Platform pad module, see pad.h. */
#include "pad.h"
#include <string.h>

static PlatPad g_pad[PLAT_PAD_PORTS] = {
    { 0xFFFF, 0x80, 0x80, 0x80, 0x80, 0, 0 },
    { 0xFFFF, 0x80, 0x80, 0x80, 0x80, 0, 0 },
};

void plat_pad_set(int port, const PlatPad *state) {
    if (port < 0 || port >= PLAT_PAD_PORTS || !state) return;
    g_pad[port] = *state;
}

int plat_pad_get(int port, PlatPad *out) {
    if (port < 0 || port >= PLAT_PAD_PORTS || !out) return 0;
    *out = g_pad[port];
    return 1;
}

uint16_t plat_pad_buttons(int port) {
    if (port < 0 || port >= PLAT_PAD_PORTS || !g_pad[port].connected) return 0xFFFF;
    return g_pad[port].buttons;
}

size_t plat_pad_report(int port, uint8_t *out, size_t cap) {
    if (port < 0 || port >= PLAT_PAD_PORTS) return 0;
    const PlatPad *p = &g_pad[port];
    size_t need = p->analog ? 8 : 4;
    if (cap < need) return 0;
    if (!p->connected) { memset(out, 0xFF, need); return need; }
    out[0] = 0x00;
    out[1] = p->analog ? 0x73 : 0x41;
    out[2] = (uint8_t)(p->buttons & 0xFF);
    out[3] = (uint8_t)(p->buttons >> 8);
    if (p->analog) { out[4] = p->rx; out[5] = p->ry; out[6] = p->lx; out[7] = p->ly; }
    return need;
}
