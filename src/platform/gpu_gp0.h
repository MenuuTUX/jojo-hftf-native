/* GP0 packet decoder (final-target code: plain C99, standard headers only, no PS1 types, no guest addresses, no global
 * state).
 *
 * Input is a stream of GP0 words, either a flat run or the nodes of an ordering table. An ordering-table node is one
 * header word (next node address in bits 0-23, number of packet words in bits 24-31) followed by that many words.
 * Node addresses are opaque 32-bit values; they are only passed back to the caller's reader callback.
 *
 * Output is an array of plain draw items in issue order, plus a payload array holding the pixels of CPU->VRAM
 * transfers. Every item carries the drawing offset, drawing area, texture window and mask state that were active
 * when it was decoded. Textures are not sampled; their page, CLUT and coordinates are reported.
 *
 * Packet layouts follow the psx-spx GPU chapter as implemented in runtime/src/gpu.c (command lengths) and
 * tools/psx_gpu_frame.py (field positions). Decoding is strictly sequential: environment commands change the state
 * for every item after them, and a textured polygon latches its own texpage word into the state, as the hardware
 * does. */
#ifndef PLAT_GPU_GP0_H
#define PLAT_GPU_GP0_H

#include <stddef.h>
#include <stdint.h>

/* Reads the 32-bit word at addr. addr is opaque to this module. */
typedef uint32_t (*GpReadWordFn)(void *ctx, uint32_t addr);

#define GP_OK          0
#define GP_ERR_FULL   (-1)   /* item or payload array full; decoding stopped, items already written are valid */
#define GP_ERR_NODES  (-2)   /* ordering-table walk reached max_nodes (probable loop) */
#define GP_ERR_ARG    (-3)   /* NULL pointer or words == NULL with n > 0 */

typedef enum {
    GP_ITEM_TRI = 1,        /* 3 vertices; flat or gouraud, textured or not */
    GP_ITEM_QUAD,           /* 4 vertices in strip order (0,1,2) and (1,2,3) */
    GP_ITEM_LINE,           /* 2 vertices; a polyline is one LINE item per segment, all flagged GP_F_POLYLINE */
    GP_ITEM_RECT,           /* origin x[0],y[0] and size w,h; flat or textured */
    GP_ITEM_FILL,           /* GP0(02). Raw VRAM coordinates, no drawing offset or area */
    GP_ITEM_COPY_VRAM,      /* GP0(80). source x[0],y[0]; destination x[1],y[1]; size w,h */
    GP_ITEM_COPY_TO_VRAM,   /* GP0(A0). destination x[0],y[0]; size w,h; pixels in payload[payload_off..] */
    GP_ITEM_COPY_TO_CPU,    /* GP0(C0). source x[0],y[0]; size w,h. The pixels come back through GPUREAD, not here */
    GP_ITEM_ENV             /* GP0(E1)..GP0(E6). env_op and env_word; the item also carries the state after it */
} GpItemType;

/* flags */
#define GP_F_GOURAUD    0x0001u   /* per-vertex colours in r/g/b */
#define GP_F_TEXTURED   0x0002u   /* u/v, clut and tpage are valid */
#define GP_F_SEMI       0x0004u   /* semi-transparency enabled for this primitive */
#define GP_F_RAW_TEX    0x0008u   /* raw texture (no colour modulation); only set together with GP_F_TEXTURED */
#define GP_F_POLYLINE   0x0010u   /* a LINE segment that came from a polyline */
#define GP_F_VAR_SIZE   0x0020u   /* a RECT whose size came from its packet (not 1, 8 or 16) */
#define GP_F_TRUNCATED  0x0040u   /* a transfer whose payload ran past the end of its node (payload_n < needed) */
#define GP_F_MASK_SET   0x0080u   /* E6 set-mask-bit state when decoded (draw items and transfers; fill ignores it) */
#define GP_F_MASK_CHECK 0x0100u   /* E6 check-mask-bit state when decoded */
#define GP_F_DITHER     0x0200u   /* E1 dither enable state */

typedef struct {
    uint8_t  type;             /* GpItemType */
    uint8_t  vert_count;       /* TRI 3, QUAD 4, LINE 2, otherwise 0 */
    uint8_t  semi_mode;        /* 0..3 active semi-transparency mode (0: 0.5B+0.5F, 1: B+F, 2: B-F, 3: B+0.25F) */
    uint8_t  env_op;           /* ENV only: 0xE1..0xE6 */
    uint16_t flags;            /* GP_F_* */
    int16_t  x[4], y[4];       /* positions, drawing offset already added (see GpItemType for FILL and COPY) */
    uint8_t  r[4], g[4], b[4]; /* 8-bit colour per vertex; flat primitives repeat the one colour */
    uint8_t  u[4], v[4];       /* texture coordinates per vertex */
    uint16_t clut;             /* textured: CLUT word (x/16 in bits 0-5, y in bits 6-14) */
    uint16_t tpage;            /* texpage state: the word latched by the most recent textured polygon, or set by E1 */
    uint16_t w, h;             /* RECT, COPY, transfers: size (copy and transfer sizes are resolved, 0 means full).
                                  RECT: 0 means nothing drawn. FILL: raw field values, 0 means nothing filled */
    int16_t  off_x, off_y;     /* drawing offset (E5) active when decoded */
    uint16_t area_x0, area_y0; /* drawing area top-left (E3), inclusive */
    uint16_t area_x1, area_y1; /* drawing area bottom-right (E4), inclusive */
    uint32_t tex_window;       /* texture window (E2) active when decoded, 20 bits */
    uint32_t env_word;         /* ENV only: operand word, low 24 bits */
    uint32_t payload_off;      /* COPY_TO_VRAM only: index of the first pixel word in GpOut.payload */
    uint32_t payload_n;        /* COPY_TO_VRAM only: payload words stored (fewer than needed if GP_F_TRUNCATED) */
} GpItem;

typedef struct {
    GpItem   *items;           /* caller-owned */
    size_t    items_cap;
    size_t    items_n;
    uint32_t *payload;         /* caller-owned */
    size_t    payload_cap;
    size_t    payload_n;
    /* statistics for this decode */
    size_t    nodes;           /* ordering-table nodes walked */
    size_t    commands;        /* GP0 commands seen, including nops and unknown ones */
    size_t    unknown;         /* opcodes with no defined length (one word skipped each) */
    size_t    truncated;       /* commands cut short by the end of their run or node (not emitted, except transfers) */
} GpOut;

/* Draw state carried from packet to packet. */
typedef struct {
    int16_t  off_x, off_y;     /* E5 */
    uint16_t area_x0, area_y0; /* E3 */
    uint16_t area_x1, area_y1; /* E4 */
    uint16_t tpage;            /* E1 low 16 bits, replaced by each textured polygon's tpage word */
    uint32_t tex_window;       /* E2, 20 bits */
    uint8_t  mask_set;         /* E6 bit 0 */
    uint8_t  mask_check;       /* E6 bit 1 */
} GpState;

void gp0_state_init(GpState *st);   /* offset 0, area (0,0)-(1023,511), everything else 0 */
void gp0_out_init(GpOut *o, GpItem *items, size_t items_cap, uint32_t *payload, size_t payload_cap);

/* Words one command occupies, header included, from its command byte alone:
 *   -1  polyline (0x48..0x4F, 0x58..0x5F): ends at a 5xxx5xxx word, length not known from the header
 *    0  undefined opcode in the reference table (0xF0..0xFE)
 *    3  CPU<->VRAM transfer header only; the pixel payload follows and its length depends on the size word
 * Every other opcode has a fixed length. */
int gp0_fixed_words(uint8_t op);

/* Decodes a flat run of GP0 words. One or more commands back to back; each command's length comes from its own
 * command byte. A command whose words run past the end is dropped and counted in o->truncated. */
int gp0_decode_words(const uint32_t *words, size_t n, GpState *st, GpOut *o);

/* Walks an ordering table. first is the address of the first node (0x00FFFFFF means an empty list). Each node is
 * read through read(); the walk ends after a node whose next field is 0xFFFFFF. max_nodes bounds the walk. */
int gp0_walk_ordering(GpReadWordFn read, void *ctx, uint32_t first, unsigned max_nodes,
                      GpState *st, GpOut *o);

#endif
