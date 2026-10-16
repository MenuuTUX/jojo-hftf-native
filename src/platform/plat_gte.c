/*
 * gte.c - PlayStation GTE (COP2) fixed-point geometry engine, plain C99.
 *
 * Structure: plat_gte_command() unpacks the 64 guest words into a local working
 * struct, runs one command on it, and packs every word back. The working
 * struct and all helpers are function-local or static; the only file-scope
 * object is the read-only reciprocal seed table below.
 *
 * Semantics follow the psx-spx GTE chapter: 44-bit MAC accumulators with
 * overflow flags, IR/colour/SZ/SXY saturation with FLAG bits, the sf (>>12)
 * and lm (clamp at 0) instruction bits, the MVMVA matrix/vector/translation
 * selects including the undocumented matrix 3 and the FC-translation quirk,
 * and the UNR reciprocal used by the perspective divide (H / SZ3).
 *
 * Where a command's flag or MAC behaviour is narrower than the full hardware
 * description (OP, SQR, GPF, GPL compute MAC with 32-bit wrap and no 44-bit
 * overflow flags), the code keeps that narrower behaviour: it is the behaviour
 * of the PSXRecomp runtime that this file is verified against (see
 * tools-phase1/gte_test/README.md).
 */
#include "plat_gte.h"

#include <stdint.h>
#include <string.h>

/* FLAG register bits (psx-spx GTE FLAG). */
#define GTE_FLAG_MAC1_POS   (1u << 30)
#define GTE_FLAG_MAC2_POS   (1u << 29)
#define GTE_FLAG_MAC3_POS   (1u << 28)
#define GTE_FLAG_MAC1_NEG   (1u << 27)
#define GTE_FLAG_MAC2_NEG   (1u << 26)
#define GTE_FLAG_MAC3_NEG   (1u << 25)
#define GTE_FLAG_IR1_SAT    (1u << 24)
#define GTE_FLAG_IR2_SAT    (1u << 23)
#define GTE_FLAG_IR3_SAT    (1u << 22)
#define GTE_FLAG_COLOR_R    (1u << 21)
#define GTE_FLAG_COLOR_G    (1u << 20)
#define GTE_FLAG_COLOR_B    (1u << 19)
#define GTE_FLAG_SZ3_OTZ    (1u << 18)
#define GTE_FLAG_DIV_OVF    (1u << 17)
#define GTE_FLAG_MAC0_POS   (1u << 16)
#define GTE_FLAG_MAC0_NEG   (1u << 15)
#define GTE_FLAG_SX2_SAT    (1u << 14)
#define GTE_FLAG_SY2_SAT    (1u << 13)
#define GTE_FLAG_IR0_SAT    (1u << 12)
#define GTE_FLAG_ERROR_MASK 0x7F87E000u
#define GTE_FLAG_ERROR      0x80000000u

/* Reciprocal seed for the UNR division: 257 entries, index = (d + 0x40) >> 7
 * over the normalised divisor d in [0x8000, 0xFFFF]. Generated from the same
 * Newton-Raphson seed formula the PSXRecomp runtime uses. */
static const uint8_t gte_unr_table[257] = {
    0xFF, 0xFD, 0xFB, 0xF9, 0xF7, 0xF5, 0xF3, 0xF1, 0xEF, 0xEE, 0xEC, 0xEA,
    0xE8, 0xE6, 0xE4, 0xE3, 0xE1, 0xDF, 0xDD, 0xDC, 0xDA, 0xD8, 0xD6, 0xD5,
    0xD3, 0xD1, 0xD0, 0xCE, 0xCD, 0xCB, 0xC9, 0xC8, 0xC6, 0xC5, 0xC3, 0xC1,
    0xC0, 0xBE, 0xBD, 0xBB, 0xBA, 0xB8, 0xB7, 0xB5, 0xB4, 0xB2, 0xB1, 0xB0,
    0xAE, 0xAD, 0xAB, 0xAA, 0xA9, 0xA7, 0xA6, 0xA4, 0xA3, 0xA2, 0xA0, 0x9F,
    0x9E, 0x9C, 0x9B, 0x9A, 0x99, 0x97, 0x96, 0x95, 0x94, 0x92, 0x91, 0x90,
    0x8F, 0x8D, 0x8C, 0x8B, 0x8A, 0x89, 0x87, 0x86, 0x85, 0x84, 0x83, 0x82,
    0x81, 0x7F, 0x7E, 0x7D, 0x7C, 0x7B, 0x7A, 0x79, 0x78, 0x77, 0x75, 0x74,
    0x73, 0x72, 0x71, 0x70, 0x6F, 0x6E, 0x6D, 0x6C, 0x6B, 0x6A, 0x69, 0x68,
    0x67, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x60, 0x5F, 0x5E, 0x5D, 0x5D,
    0x5C, 0x5B, 0x5A, 0x59, 0x58, 0x57, 0x56, 0x55, 0x54, 0x53, 0x53, 0x52,
    0x51, 0x50, 0x4F, 0x4E, 0x4D, 0x4D, 0x4C, 0x4B, 0x4A, 0x49, 0x48, 0x48,
    0x47, 0x46, 0x45, 0x44, 0x43, 0x43, 0x42, 0x41, 0x40, 0x3F, 0x3F, 0x3E,
    0x3D, 0x3C, 0x3C, 0x3B, 0x3A, 0x39, 0x39, 0x38, 0x37, 0x36, 0x36, 0x35,
    0x34, 0x33, 0x33, 0x32, 0x31, 0x31, 0x30, 0x2F, 0x2E, 0x2E, 0x2D, 0x2C,
    0x2C, 0x2B, 0x2A, 0x2A, 0x29, 0x28, 0x28, 0x27, 0x26, 0x26, 0x25, 0x24,
    0x24, 0x23, 0x22, 0x22, 0x21, 0x20, 0x20, 0x1F, 0x1E, 0x1E, 0x1D, 0x1D,
    0x1C, 0x1B, 0x1B, 0x1A, 0x19, 0x19, 0x18, 0x18, 0x17, 0x16, 0x16, 0x15,
    0x15, 0x14, 0x14, 0x13, 0x12, 0x12, 0x11, 0x11, 0x10, 0x0F, 0x0F, 0x0E,
    0x0E, 0x0D, 0x0D, 0x0C, 0x0C, 0x0B, 0x0A, 0x0A, 0x09, 0x09, 0x08, 0x08,
    0x07, 0x07, 0x06, 0x06, 0x05, 0x05, 0x04, 0x04, 0x03, 0x03, 0x02, 0x02,
    0x01, 0x01, 0x00, 0x00, 0x00
};

/* Working unpacked state for one command. Mixed-width guest words are split
 * into typed fields here and repacked by gte_export(). */
typedef struct gte_core {
    int16_t  V[3][3];    /* V[0]=V0, V[1]=V1, V[2]=V2 */
    uint32_t SXY[4];     /* packed (y << 16) | x; SXY[3] mirrors SXY[2] */
    uint16_t SZ[4];
    uint32_t RGBC;
    uint32_t RGB[3];
    int16_t  IR[4];      /* IR[0]=IR0 .. IR[3]=IR3 */
    int32_t  MAC[4];     /* MAC[0]=MAC0 .. MAC[3]=MAC3 */
    int16_t  RT[3][3];
    int16_t  L[3][3];
    int16_t  LC[3][3];
    int32_t  TR[3];
    int32_t  BK[3];
    int32_t  FC[3];
    int32_t  OFX, OFY;
    uint16_t H;
    int16_t  DQA;
    int32_t  DQB;
    int16_t  ZSF3, ZSF4;
    uint16_t OTZ;
    uint32_t FLAG;
} gte_core;

/* ------------------------------------------------------------------------- */
/* Word helpers                                                              */
/* ------------------------------------------------------------------------- */

static int16_t gte_lo16(uint32_t w) { return (int16_t)(uint16_t)(w & 0xFFFFu); }
static int16_t gte_hi16(uint32_t w) { return (int16_t)(uint16_t)(w >> 16); }

/* Sign-extended 16-bit value as a 32-bit word. */
static uint32_t gte_sext16(uint32_t w) { return (uint32_t)(int32_t)gte_lo16(w); }

static uint32_t gte_pack_pair(int16_t lo, int16_t hi) {
    return (uint32_t)(uint16_t)lo | ((uint32_t)(uint16_t)hi << 16);
}

/* Count of leading bits equal to the sign bit (LZCR), 32 for zero. */
static uint32_t gte_lzcr(uint32_t value) {
    uint32_t bits = (value & 0x80000000u) ? ~value : value;
    uint32_t count = 0;
    if (bits == 0) return 32;
    while ((bits & 0x80000000u) == 0) {
        bits <<= 1;
        ++count;
    }
    return count;
}

/* One 5-bit IRGB component from a signed IR value (IR >> 7, clamped 0..31). */
static uint32_t gte_irgb_component(int16_t ir) {
    uint32_t scaled;
    if (ir <= 0) return 0;
    scaled = (uint32_t)ir >> 7;
    return scaled > 0x1Fu ? 0x1Fu : scaled;
}

/* IRGB/ORGB word derived from the current IR1..IR3 in the data words. */
static uint32_t gte_irgb_from_data(const uint32_t d[32]) {
    uint32_t r = gte_irgb_component(gte_lo16(d[9]));
    uint32_t g = gte_irgb_component(gte_lo16(d[10]));
    uint32_t b = gte_irgb_component(gte_lo16(d[11]));
    return (b << 10) | (g << 5) | r;
}

/* Bring the guest words to canonical form: the sign-extended / masked forms
 * that every accessor and command reads back. Run before each register write,
 * matching the PSXRecomp runtime. */
static void gte_canonicalize(plat_gte_state *s) {
    uint32_t *d = s->data;
    uint32_t *c = s->ctrl;
    d[7]  &= 0xFFFFu;
    d[16] &= 0xFFFFu;
    d[17] &= 0xFFFFu;
    d[18] &= 0xFFFFu;
    d[19] &= 0xFFFFu;
    d[1]  = gte_sext16(d[1]);
    d[3]  = gte_sext16(d[3]);
    d[5]  = gte_sext16(d[5]);
    d[8]  = gte_sext16(d[8]);
    d[9]  = gte_sext16(d[9]);
    d[10] = gte_sext16(d[10]);
    d[11] = gte_sext16(d[11]);
    d[15] = d[14];
    d[28] = d[29] = gte_irgb_from_data(d);
    d[31] = gte_lzcr(d[30]);
    c[4]  = gte_sext16(c[4]);
    c[12] = gte_sext16(c[12]);
    c[20] = gte_sext16(c[20]);
    c[26] = gte_sext16(c[26]);
    c[27] = gte_sext16(c[27]);
    c[29] = gte_sext16(c[29]);
    c[30] = gte_sext16(c[30]);
}

/* ------------------------------------------------------------------------- */
/* Register accessors                                                        */
/* ------------------------------------------------------------------------- */

void plat_gte_init(plat_gte_state *s) {
    memset(s, 0, sizeof *s);
}

uint32_t plat_gte_read_data(const plat_gte_state *s, unsigned reg) {
    const uint32_t *d = s->data;
    if (reg >= 32) return 0;
    switch (reg) {
    case 7: case 16: case 17: case 18: case 19:
        return d[reg] & 0xFFFFu;
    case 1: case 3: case 5: case 8: case 9: case 10: case 11:
        return gte_sext16(d[reg]);
    case 15:
        return d[14];
    case 28: case 29:
        return gte_irgb_from_data(d);
    case 31:
        return gte_lzcr(d[30]);
    default:
        return d[reg];
    }
}

uint32_t plat_gte_read_ctrl(const plat_gte_state *s, unsigned reg) {
    const uint32_t *c = s->ctrl;
    if (reg >= 32) return 0;
    switch (reg) {
    case 4: case 12: case 20: case 26: case 27: case 29: case 30:
        return gte_sext16(c[reg]);
    default:
        return c[reg];
    }
}

void plat_gte_write_data(plat_gte_state *s, unsigned reg, uint32_t value) {
    uint32_t *d = s->data;
    if (reg >= 32) return;
    gte_canonicalize(s);
    switch (reg) {
    case 7: case 16: case 17: case 18: case 19:
        d[reg] = value & 0xFFFFu;
        break;
    case 1: case 3: case 5: case 8: case 9: case 10: case 11:
        d[reg] = gte_sext16(value);
        if (reg >= 9) {
            uint32_t packed = gte_irgb_from_data(d);
            d[28] = packed;
            d[29] = packed;
        }
        break;
    case 12: case 13:
        d[reg] = value;
        break;
    case 14:
        d[14] = value;
        d[15] = value;
        break;
    case 15:                       /* SXYP write pushes the SXY FIFO */
        d[12] = d[13];
        d[13] = d[14];
        d[14] = value;
        d[15] = value;
        break;
    case 28:                       /* IRGB write sets IR1..IR3 from 5-bit fields */
        d[9]  = (value & 0x1Fu) << 7;
        d[10] = ((value >> 5) & 0x1Fu) << 7;
        d[11] = ((value >> 10) & 0x1Fu) << 7;
        d[28] = value & 0x7FFFu;
        d[29] = value & 0x7FFFu;
        break;
    case 29: {                     /* ORGB is read-only */
        uint32_t packed = gte_irgb_from_data(d);
        d[28] = packed;
        d[29] = packed;
        break;
    }
    case 30:
        d[30] = value;
        d[31] = gte_lzcr(value);
        break;
    case 31:                       /* LZCR is read-only */
        d[31] = gte_lzcr(d[30]);
        break;
    default:
        d[reg] = value;
        break;
    }
}

void plat_gte_write_ctrl(plat_gte_state *s, unsigned reg, uint32_t value) {
    uint32_t *c = s->ctrl;
    if (reg >= 32) return;
    gte_canonicalize(s);
    switch (reg) {
    case 4: case 12: case 20: case 26: case 27: case 29: case 30:
        c[reg] = gte_sext16(value);
        break;
    case 31:                       /* FLAG: writable bits, error summary recomputed */
        c[31] = value & 0x7FFFF000u;
        if (c[31] & GTE_FLAG_ERROR_MASK) c[31] |= GTE_FLAG_ERROR;
        break;
    default:
        c[reg] = value;
        break;
    }
}

/* ------------------------------------------------------------------------- */
/* Pack / unpack between guest words and gte_core                            */
/* ------------------------------------------------------------------------- */

static void gte_import(gte_core *g, const plat_gte_state *s) {
    const uint32_t *d = s->data;
    const uint32_t *c = s->ctrl;
    int i;

    g->V[0][0] = gte_lo16(d[0]); g->V[0][1] = gte_hi16(d[0]); g->V[0][2] = gte_lo16(d[1]);
    g->V[1][0] = gte_lo16(d[2]); g->V[1][1] = gte_hi16(d[2]); g->V[1][2] = gte_lo16(d[3]);
    g->V[2][0] = gte_lo16(d[4]); g->V[2][1] = gte_hi16(d[4]); g->V[2][2] = gte_lo16(d[5]);
    g->RGBC = d[6];
    g->OTZ = (uint16_t)(d[7] & 0xFFFFu);
    g->IR[0] = gte_lo16(d[8]);
    g->IR[1] = gte_lo16(d[9]);
    g->IR[2] = gte_lo16(d[10]);
    g->IR[3] = gte_lo16(d[11]);
    g->SXY[0] = d[12];
    g->SXY[1] = d[13];
    g->SXY[2] = d[14];
    g->SXY[3] = d[14];
    for (i = 0; i < 4; ++i) g->SZ[i] = (uint16_t)(d[16 + i] & 0xFFFFu);
    for (i = 0; i < 3; ++i) g->RGB[i] = d[20 + i];
    g->MAC[0] = (int32_t)d[24];
    g->MAC[1] = (int32_t)d[25];
    g->MAC[2] = (int32_t)d[26];
    g->MAC[3] = (int32_t)d[27];

    g->RT[0][0] = gte_lo16(c[0]); g->RT[0][1] = gte_hi16(c[0]); g->RT[0][2] = gte_lo16(c[1]);
    g->RT[1][0] = gte_hi16(c[1]); g->RT[1][1] = gte_lo16(c[2]); g->RT[1][2] = gte_hi16(c[2]);
    g->RT[2][0] = gte_lo16(c[3]); g->RT[2][1] = gte_hi16(c[3]); g->RT[2][2] = gte_lo16(c[4]);
    for (i = 0; i < 3; ++i) g->TR[i] = (int32_t)c[5 + i];

    g->L[0][0] = gte_lo16(c[8]);  g->L[0][1] = gte_hi16(c[8]);  g->L[0][2] = gte_lo16(c[9]);
    g->L[1][0] = gte_hi16(c[9]);  g->L[1][1] = gte_lo16(c[10]); g->L[1][2] = gte_hi16(c[10]);
    g->L[2][0] = gte_lo16(c[11]); g->L[2][1] = gte_hi16(c[11]); g->L[2][2] = gte_lo16(c[12]);
    for (i = 0; i < 3; ++i) g->BK[i] = (int32_t)c[13 + i];

    g->LC[0][0] = gte_lo16(c[16]); g->LC[0][1] = gte_hi16(c[16]); g->LC[0][2] = gte_lo16(c[17]);
    g->LC[1][0] = gte_hi16(c[17]); g->LC[1][1] = gte_lo16(c[18]); g->LC[1][2] = gte_hi16(c[18]);
    g->LC[2][0] = gte_lo16(c[19]); g->LC[2][1] = gte_hi16(c[19]); g->LC[2][2] = gte_lo16(c[20]);
    for (i = 0; i < 3; ++i) g->FC[i] = (int32_t)c[21 + i];
    g->OFX = (int32_t)c[24];
    g->OFY = (int32_t)c[25];
    g->H = (uint16_t)(c[26] & 0xFFFFu);
    g->DQA = gte_lo16(c[27]);
    g->DQB = (int32_t)c[28];
    g->ZSF3 = gte_lo16(c[29]);
    g->ZSF4 = gte_lo16(c[30]);
    g->FLAG = c[31] & 0x7FFFF000u;

}

static void gte_export(plat_gte_state *s, const gte_core *g) {
    uint32_t *d = s->data;
    uint32_t *c = s->ctrl;
    uint32_t packed_irgb;
    int i;

    d[0] = gte_pack_pair(g->V[0][0], g->V[0][1]);
    d[1] = (uint32_t)(int32_t)g->V[0][2];
    d[2] = gte_pack_pair(g->V[1][0], g->V[1][1]);
    d[3] = (uint32_t)(int32_t)g->V[1][2];
    d[4] = gte_pack_pair(g->V[2][0], g->V[2][1]);
    d[5] = (uint32_t)(int32_t)g->V[2][2];
    d[6] = g->RGBC;
    d[7] = g->OTZ;
    d[8]  = (uint32_t)(int32_t)g->IR[0];
    d[9]  = (uint32_t)(int32_t)g->IR[1];
    d[10] = (uint32_t)(int32_t)g->IR[2];
    d[11] = (uint32_t)(int32_t)g->IR[3];
    d[12] = g->SXY[0];
    d[13] = g->SXY[1];
    d[14] = g->SXY[2];
    d[15] = g->SXY[3];
    for (i = 0; i < 4; ++i) d[16 + i] = g->SZ[i];
    for (i = 0; i < 3; ++i) d[20 + i] = g->RGB[i];
    /* d[23] (RES1), d[30] (LZCS) are not touched by any command. */
    d[24] = (uint32_t)g->MAC[0];
    d[25] = (uint32_t)g->MAC[1];
    d[26] = (uint32_t)g->MAC[2];
    d[27] = (uint32_t)g->MAC[3];
    packed_irgb = (gte_irgb_component(g->IR[3]) << 10) |
                  (gte_irgb_component(g->IR[2]) << 5) |
                  gte_irgb_component(g->IR[1]);
    d[28] = packed_irgb;
    d[29] = packed_irgb;
    d[31] = gte_lzcr(d[30]);

    c[0] = gte_pack_pair(g->RT[0][0], g->RT[0][1]);
    c[1] = gte_pack_pair(g->RT[0][2], g->RT[1][0]);
    c[2] = gte_pack_pair(g->RT[1][1], g->RT[1][2]);
    c[3] = gte_pack_pair(g->RT[2][0], g->RT[2][1]);
    c[4] = (uint32_t)(int32_t)g->RT[2][2];
    for (i = 0; i < 3; ++i) c[5 + i] = (uint32_t)g->TR[i];
    c[8]  = gte_pack_pair(g->L[0][0], g->L[0][1]);
    c[9]  = gte_pack_pair(g->L[0][2], g->L[1][0]);
    c[10] = gte_pack_pair(g->L[1][1], g->L[1][2]);
    c[11] = gte_pack_pair(g->L[2][0], g->L[2][1]);
    c[12] = (uint32_t)(int32_t)g->L[2][2];
    for (i = 0; i < 3; ++i) c[13 + i] = (uint32_t)g->BK[i];
    c[16] = gte_pack_pair(g->LC[0][0], g->LC[0][1]);
    c[17] = gte_pack_pair(g->LC[0][2], g->LC[1][0]);
    c[18] = gte_pack_pair(g->LC[1][1], g->LC[1][2]);
    c[19] = gte_pack_pair(g->LC[2][0], g->LC[2][1]);
    c[20] = (uint32_t)(int32_t)g->LC[2][2];
    for (i = 0; i < 3; ++i) c[21 + i] = (uint32_t)g->FC[i];
    c[24] = (uint32_t)g->OFX;
    c[25] = (uint32_t)g->OFY;
    c[26] = (uint32_t)(int32_t)(int16_t)g->H;
    c[27] = (uint32_t)(int32_t)g->DQA;
    c[28] = (uint32_t)g->DQB;
    c[29] = (uint32_t)(int32_t)g->ZSF3;
    c[30] = (uint32_t)(int32_t)g->ZSF4;
    c[31] = g->FLAG;
}

/* ------------------------------------------------------------------------- */
/* Arithmetic primitives                                                     */
/* ------------------------------------------------------------------------- */

/* Flag MAC overflow beyond the 44-bit signed accumulator range. n = 1..3. */
static void gte_check_mac_overflow(gte_core *g, int64_t value, int n) {
    if (value > 0x7FFFFFFFFFFLL)  g->FLAG |= 1u << (31 - n);
    if (value < -0x80000000000LL) g->FLAG |= 1u << (28 - n);
}

static void gte_check_mac0_overflow(gte_core *g, int64_t value) {
    if (value > 0x7FFFFFFFLL)  g->FLAG |= GTE_FLAG_MAC0_POS;
    if (value < -0x80000000LL) g->FLAG |= GTE_FLAG_MAC0_NEG;
}

/* Flag and wrap an intermediate MAC value to 44 bits (sign-extended). */
static int64_t gte_mac44(gte_core *g, int64_t value, int n) {
    const uint64_t mask = (1ull << 44) - 1ull;
    const uint64_t sign = 1ull << 43;
    uint64_t wrapped;
    gte_check_mac_overflow(g, value, n);
    wrapped = (uint64_t)value & mask;
    if (wrapped & sign) wrapped |= ~mask;
    return (int64_t)wrapped;
}

/* IR1..IR3 saturation. lm selects the lower bound (0 instead of -0x8000). */
static int16_t gte_sat_ir(gte_core *g, int32_t value, int n, int lm) {
    const int32_t lo = lm ? 0 : -0x8000;
    if (value < lo) {
        g->FLAG |= 1u << (25 - n);
        return (int16_t)lo;
    }
    if (value > 0x7FFF) {
        g->FLAG |= 1u << (25 - n);
        return 0x7FFF;
    }
    return (int16_t)value;
}

static int16_t gte_sat_ir0(gte_core *g, int32_t value) {
    if (value < 0)      { g->FLAG |= GTE_FLAG_IR0_SAT; return 0; }
    if (value > 0x1000) { g->FLAG |= GTE_FLAG_IR0_SAT; return 0x1000; }
    return (int16_t)value;
}

/* Colour saturation, comp 0=R 1=G 2=B. */
static uint8_t gte_sat_color(gte_core *g, int32_t value, int comp) {
    if (value < 0)   { g->FLAG |= 1u << (21 - comp); return 0; }
    if (value > 255) { g->FLAG |= 1u << (21 - comp); return 255; }
    return (uint8_t)value;
}

static uint16_t gte_sat_sz(gte_core *g, int32_t value) {
    if (value < 0)      { g->FLAG |= GTE_FLAG_SZ3_OTZ; return 0; }
    if (value > 0xFFFF) { g->FLAG |= GTE_FLAG_SZ3_OTZ; return 0xFFFF; }
    return (uint16_t)value;
}

static void gte_push_sz(gte_core *g, int32_t value) {
    g->SZ[0] = g->SZ[1];
    g->SZ[1] = g->SZ[2];
    g->SZ[2] = g->SZ[3];
    g->SZ[3] = gte_sat_sz(g, value);
}

static void gte_push_sxy(gte_core *g, int64_t sx, int64_t sy) {
    uint16_t sx_sat, sy_sat;
    if (sx < -0x400)     { sx_sat = (uint16_t)(int16_t)-0x400; g->FLAG |= GTE_FLAG_SX2_SAT; }
    else if (sx > 0x3FF) { sx_sat = 0x3FF; g->FLAG |= GTE_FLAG_SX2_SAT; }
    else                 { sx_sat = (uint16_t)(int16_t)sx; }
    if (sy < -0x400)     { sy_sat = (uint16_t)(int16_t)-0x400; g->FLAG |= GTE_FLAG_SY2_SAT; }
    else if (sy > 0x3FF) { sy_sat = 0x3FF; g->FLAG |= GTE_FLAG_SY2_SAT; }
    else                 { sy_sat = (uint16_t)(int16_t)sy; }
    g->SXY[0] = g->SXY[1];
    g->SXY[1] = g->SXY[2];
    g->SXY[2] = ((uint32_t)sy_sat << 16) | (uint32_t)sx_sat;
    g->SXY[3] = g->SXY[2];
}

/* ------------------------------------------------------------------------- */
/* Perspective divide: H / SZ3 with the UNR reciprocal                       */
/* ------------------------------------------------------------------------- */

static uint32_t gte_clz16(uint16_t v) {
    uint32_t n = 0;
    int b;
    for (b = 15; b >= 0; --b) {
        if (v & (1u << b)) break;
        ++n;
    }
    return n;
}

static int32_t gte_calc_recip(uint16_t divisor) {
    int32_t x = 0x101 + (int32_t)gte_unr_table[((divisor & 0x7FFF) + 0x40) >> 7];
    int32_t tmp = (((int32_t)divisor * -x) + 0x80) >> 8;
    int32_t tmp2 = ((x * (131072 + tmp)) + 0x80) >> 8;
    return tmp2;
}

static int32_t gte_divide(gte_core *g, uint16_t h, uint16_t sz3) {
    uint32_t bias, dividend, divisor, result;
    if ((uint32_t)sz3 * 2u <= (uint32_t)h) {
        g->FLAG |= GTE_FLAG_DIV_OVF;
        return 0x1FFFF;
    }
    bias = gte_clz16(sz3);
    dividend = (uint32_t)h << bias;
    divisor = (uint32_t)sz3 << bias;
    result = (uint32_t)((((uint64_t)dividend *
                          (uint32_t)gte_calc_recip((uint16_t)(divisor | 0x8000u))) +
                         32768u) >> 16);
    if (result > 0x1FFFFu) result = 0x1FFFFu;
    return (int32_t)result;
}

/* ------------------------------------------------------------------------- */
/* Shared pipeline stages                                                    */
/* ------------------------------------------------------------------------- */

/* MAC[n] = acc >> shift, IR[n] = saturate(MAC[n], lm). */
static void gte_store_mac_ir(gte_core *g, int n, int64_t value, int shift, int lm) {
    int32_t mac;
    value = gte_mac44(g, value, n) >> shift;
    mac = (int32_t)value;
    g->MAC[n] = mac;
    g->IR[n] = gte_sat_ir(g, mac, n, lm);
}

static void gte_light_transform(gte_core *g, const int16_t v[3], int shift, int lm) {
    int row;
    for (row = 0; row < 3; ++row) {
        int64_t acc = gte_mac44(g,
            (int64_t)g->L[row][0] * v[0] + (int64_t)g->L[row][1] * v[1], row + 1);
        gte_store_mac_ir(g, row + 1,
            acc + (int64_t)g->L[row][2] * v[2], shift, lm);
    }
}

static void gte_light_color(gte_core *g, int shift, int lm) {
    const int16_t in[3] = { g->IR[1], g->IR[2], g->IR[3] };
    int row;
    for (row = 0; row < 3; ++row) {
        int64_t acc = gte_mac44(g,
            (int64_t)g->BK[row] * 4096 + (int64_t)g->LC[row][0] * in[0], row + 1);
        acc = gte_mac44(g, acc + (int64_t)g->LC[row][1] * in[1], row + 1);
        gte_store_mac_ir(g, row + 1,
            acc + (int64_t)g->LC[row][2] * in[2], shift, lm);
    }
}

/* IR * RGBC colour multiply (CC / NCCS path). */
static void gte_color_multiply(gte_core *g, int shift, int lm) {
    const int16_t in[3] = { g->IR[1], g->IR[2], g->IR[3] };
    const int64_t r0 = g->RGBC & 0xFFu;
    const int64_t g0 = (g->RGBC >> 8) & 0xFFu;
    const int64_t b0 = (g->RGBC >> 16) & 0xFFu;
    gte_store_mac_ir(g, 1, r0 * in[0] * 16, shift, lm);
    gte_store_mac_ir(g, 2, g0 * in[1] * 16, shift, lm);
    gte_store_mac_ir(g, 3, b0 * in[2] * 16, shift, lm);
}

static void gte_push_rgb_from_mac(gte_core *g) {
    uint8_t r = gte_sat_color(g, g->MAC[1] >> 4, 0);
    uint8_t gg = gte_sat_color(g, g->MAC[2] >> 4, 1);
    uint8_t b = gte_sat_color(g, g->MAC[3] >> 4, 2);
    uint32_t code = (g->RGBC >> 24) & 0xFFu;
    g->RGB[0] = g->RGB[1];
    g->RGB[1] = g->RGB[2];
    g->RGB[2] = (code << 24) | ((uint32_t)b << 16) | ((uint32_t)gg << 8) | (uint32_t)r;
}

/* Interpolate toward the far colour FC by IR0. base[] are unshifted MAC inputs. */
static void gte_depth_cue(gte_core *g, const int64_t base[3], int shift, int lm) {
    int i;
    int16_t step[3];
    for (i = 0; i < 3; ++i)
        gte_store_mac_ir(g, i + 1, (int64_t)g->FC[i] * 4096 - base[i], shift, 0);
    step[0] = g->IR[1];
    step[1] = g->IR[2];
    step[2] = g->IR[3];
    for (i = 0; i < 3; ++i)
        gte_store_mac_ir(g, i + 1, base[i] + (int64_t)g->IR[0] * step[i], shift, lm);
}

/* ------------------------------------------------------------------------- */
/* Commands                                                                  */
/* ------------------------------------------------------------------------- */

static void gte_set_error(gte_core *g) {
    if (g->FLAG & GTE_FLAG_ERROR_MASK) g->FLAG |= GTE_FLAG_ERROR;
}

/* One perspective transform of vertex v. */
static void gte_rtps_one(gte_core *g, const int16_t v[3], int set_mac0, int shift, int lm) {
    int64_t m[4];
    int row;
    int64_t sx16, sy16, xterm, mac0;
    int32_t h_div;

    for (row = 0; row < 3; ++row) {
        const int n = row + 1;
        int64_t acc = (int64_t)g->TR[row] * 4096 + (int64_t)g->RT[row][0] * v[0];
        acc = gte_mac44(g, acc, n);
        acc += (int64_t)g->RT[row][1] * v[1];
        acc = gte_mac44(g, acc, n);
        m[n] = acc + (int64_t)g->RT[row][2] * v[2];
    }

    gte_check_mac_overflow(g, m[1], 1);
    gte_check_mac_overflow(g, m[2], 2);
    gte_check_mac_overflow(g, m[3], 3);
    g->MAC[1] = (int32_t)(m[1] >> shift);
    g->MAC[2] = (int32_t)(m[2] >> shift);
    g->MAC[3] = (int32_t)(m[3] >> shift);

    g->IR[1] = gte_sat_ir(g, g->MAC[1], 1, lm);
    g->IR[2] = gte_sat_ir(g, g->MAC[2], 2, lm);
    /* IR3 flag is taken from the unshifted depth with lm = 0; the stored IR3
     * is clamped from MAC3 using the real lm bit. */
    (void)gte_sat_ir(g, (int32_t)(m[3] >> 12), 3, 0);
    {
        int32_t ir3 = g->MAC[3];
        const int32_t lo = lm ? 0 : -0x8000;
        if (ir3 < lo) ir3 = lo;
        if (ir3 > 0x7FFF) ir3 = 0x7FFF;
        g->IR[3] = (int16_t)ir3;
    }

    /* SZ FIFO takes the depth as MAC3 >> 12 regardless of sf. */
    gte_push_sz(g, (int32_t)(m[3] >> 12));

    h_div = gte_divide(g, g->H, g->SZ[3]);

    xterm = (int64_t)g->IR[1] * h_div;
    sx16 = (int64_t)g->OFX + xterm;
    sy16 = (int64_t)g->OFY + (int64_t)g->IR[2] * h_div;
    gte_check_mac0_overflow(g, sx16);
    gte_check_mac0_overflow(g, sy16);
    gte_push_sxy(g, sx16 >> 16, sy16 >> 16);

    if (set_mac0) {
        mac0 = (int64_t)g->DQA * h_div + g->DQB;
        gte_check_mac0_overflow(g, mac0);
        g->MAC[0] = (int32_t)mac0;
        g->IR[0] = gte_sat_ir0(g, (int32_t)(mac0 >> 12));
    }
}

static void gte_rtps(gte_core *g, int shift, int lm) {
    g->FLAG = 0;
    gte_rtps_one(g, g->V[0], 1, shift, lm);
    gte_set_error(g);
}

static void gte_rtpt(gte_core *g, int shift, int lm) {
    g->FLAG = 0;
    gte_rtps_one(g, g->V[0], 0, shift, lm);
    gte_rtps_one(g, g->V[1], 0, shift, lm);
    gte_rtps_one(g, g->V[2], 1, shift, lm);
    gte_set_error(g);
}

static void gte_nclip(gte_core *g) {
    const int64_t sx0 = gte_lo16(g->SXY[0]), sy0 = gte_hi16(g->SXY[0]);
    const int64_t sx1 = gte_lo16(g->SXY[1]), sy1 = gte_hi16(g->SXY[1]);
    const int64_t sx2 = gte_lo16(g->SXY[2]), sy2 = gte_hi16(g->SXY[2]);
    const int64_t mac0 = sx0 * (sy1 - sy2) + sx1 * (sy2 - sy0) + sx2 * (sy0 - sy1);
    g->FLAG = 0;
    gte_check_mac0_overflow(g, mac0);
    g->MAC[0] = (int32_t)mac0;
    gte_set_error(g);
}

static void gte_avsz3(gte_core *g) {
    const int64_t sum = (int64_t)g->SZ[1] + g->SZ[2] + g->SZ[3];
    const int64_t mac0 = (int64_t)g->ZSF3 * sum;
    g->FLAG = 0;
    gte_check_mac0_overflow(g, mac0);
    g->MAC[0] = (int32_t)mac0;
    g->OTZ = gte_sat_sz(g, (int32_t)(mac0 >> 12));
    gte_set_error(g);
}

static void gte_avsz4(gte_core *g) {
    const int64_t sum = (int64_t)g->SZ[0] + g->SZ[1] + g->SZ[2] + g->SZ[3];
    const int64_t mac0 = (int64_t)g->ZSF4 * sum;
    g->FLAG = 0;
    gte_check_mac0_overflow(g, mac0);
    g->MAC[0] = (int32_t)mac0;
    g->OTZ = gte_sat_sz(g, (int32_t)(mac0 >> 12));
    gte_set_error(g);
}

/* Normal-colour family. count = 1 for the Single form, 3 for the Triple form
 * (vertices V0, V1, V2 in order). */
static void gte_nccs_n(gte_core *g, int count, int shift, int lm) {
    int k;
    g->FLAG = 0;
    for (k = 0; k < count; ++k) {
        gte_light_transform(g, g->V[k], shift, lm);
        gte_light_color(g, shift, lm);
        gte_color_multiply(g, shift, lm);
        gte_push_rgb_from_mac(g);
    }
    gte_set_error(g);
}

static void gte_ncs_n(gte_core *g, int count, int shift, int lm) {
    int k;
    g->FLAG = 0;
    for (k = 0; k < count; ++k) {
        gte_light_transform(g, g->V[k], shift, lm);
        gte_light_color(g, shift, lm);
        gte_push_rgb_from_mac(g);
    }
    gte_set_error(g);
}

static void gte_ncds_n(gte_core *g, int count, int shift, int lm) {
    int k;
    g->FLAG = 0;
    for (k = 0; k < count; ++k) {
        gte_light_transform(g, g->V[k], shift, lm);
        gte_light_color(g, shift, lm);
        {
            /* Depth-cue base uses the IR just produced by light_color. */
            const int64_t r = g->RGBC & 0xFFu;
            const int64_t gg = (g->RGBC >> 8) & 0xFFu;
            const int64_t b = (g->RGBC >> 16) & 0xFFu;
            const int64_t base[3] = { r * g->IR[1] * 16, gg * g->IR[2] * 16, b * g->IR[3] * 16 };
            gte_depth_cue(g, base, shift, lm);
        }
        gte_push_rgb_from_mac(g);
    }
    gte_set_error(g);
}

static void gte_dpcs(gte_core *g, int shift, int lm) {
    const int64_t base[3] = {
        (int64_t)(g->RGBC & 0xFFu) << 16,
        (int64_t)((g->RGBC >> 8) & 0xFFu) << 16,
        (int64_t)((g->RGBC >> 16) & 0xFFu) << 16 };
    g->FLAG = 0;
    gte_depth_cue(g, base, shift, lm);
    gte_push_rgb_from_mac(g);
    gte_set_error(g);
}

static void gte_dpct(gte_core *g, int shift, int lm) {
    int i;
    g->FLAG = 0;
    for (i = 0; i < 3; ++i) {
        /* RGB[0] is re-read each pass: each push shifts the FIFO. */
        const int64_t base[3] = {
            (int64_t)(g->RGB[0] & 0xFFu) << 16,
            (int64_t)((g->RGB[0] >> 8) & 0xFFu) << 16,
            (int64_t)((g->RGB[0] >> 16) & 0xFFu) << 16 };
        gte_depth_cue(g, base, shift, lm);
        gte_push_rgb_from_mac(g);
    }
    gte_set_error(g);
}

static void gte_dpcl(gte_core *g, int shift, int lm) {
    const int64_t r = g->RGBC & 0xFFu;
    const int64_t gg = (g->RGBC >> 8) & 0xFFu;
    const int64_t b = (g->RGBC >> 16) & 0xFFu;
    const int64_t base[3] = { r * g->IR[1] * 16, gg * g->IR[2] * 16, b * g->IR[3] * 16 };
    g->FLAG = 0;
    gte_depth_cue(g, base, shift, lm);
    gte_push_rgb_from_mac(g);
    gte_set_error(g);
}

static void gte_intpl(gte_core *g, int shift, int lm) {
    const int64_t base[3] = {
        (int64_t)g->IR[1] * 4096, (int64_t)g->IR[2] * 4096, (int64_t)g->IR[3] * 4096 };
    g->FLAG = 0;
    gte_depth_cue(g, base, shift, lm);
    gte_push_rgb_from_mac(g);
    gte_set_error(g);
}

static void gte_cdp(gte_core *g, int shift, int lm) {
    const int64_t r = g->RGBC & 0xFFu;
    const int64_t gg = (g->RGBC >> 8) & 0xFFu;
    const int64_t b = (g->RGBC >> 16) & 0xFFu;
    g->FLAG = 0;
    gte_light_color(g, shift, lm);
    {
        const int64_t base[3] = { r * g->IR[1] * 16, gg * g->IR[2] * 16, b * g->IR[3] * 16 };
        gte_depth_cue(g, base, shift, lm);
    }
    gte_push_rgb_from_mac(g);
    gte_set_error(g);
}

static void gte_cc(gte_core *g, int shift, int lm) {
    g->FLAG = 0;
    gte_light_color(g, shift, lm);
    gte_color_multiply(g, shift, lm);
    gte_push_rgb_from_mac(g);
    gte_set_error(g);
}

static void gte_mvmva(gte_core *g, uint32_t instr, int shift, int lm) {
    const uint32_t mx = (instr >> 17) & 3u;   /* 0=RT 1=L 2=LC 3=undocumented */
    const uint32_t vv = (instr >> 15) & 3u;   /* 0=V0 1=V1 2=V2 3=IR */
    const uint32_t tv = (instr >> 13) & 3u;   /* 0=TR 1=BK 2=FC (quirky) 3=none */
    int16_t M[3][3];
    int16_t V[3];
    int64_t T[3];
    int row, i, j;

    g->FLAG = 0;

    switch (mx) {
    case 0:
        for (i = 0; i < 3; ++i) for (j = 0; j < 3; ++j) M[i][j] = g->RT[i][j];
        break;
    case 1:
        for (i = 0; i < 3; ++i) for (j = 0; j < 3; ++j) M[i][j] = g->L[i][j];
        break;
    case 2:
        for (i = 0; i < 3; ++i) for (j = 0; j < 3; ++j) M[i][j] = g->LC[i][j];
        break;
    default: {
        /* Undocumented matrix: built from R (RGBC low byte << 4), IR0 and the
         * diagonal of RT. Verified against hardware register vectors. */
        const int16_t color = (int16_t)(uint16_t)((g->RGBC & 0xFFu) << 4);
        M[0][0] = (int16_t)(-color);
        M[0][1] = color;
        M[0][2] = g->IR[0];
        M[1][0] = M[1][1] = M[1][2] = g->RT[0][2];
        M[2][0] = M[2][1] = M[2][2] = g->RT[1][1];
        break;
    }
    }

    switch (vv) {
    case 0:  V[0] = g->V[0][0]; V[1] = g->V[0][1]; V[2] = g->V[0][2]; break;
    case 1:  V[0] = g->V[1][0]; V[1] = g->V[1][1]; V[2] = g->V[1][2]; break;
    case 2:  V[0] = g->V[2][0]; V[1] = g->V[2][1]; V[2] = g->V[2][2]; break;
    default: V[0] = g->IR[1];   V[1] = g->IR[2];   V[2] = g->IR[3];   break;
    }

    switch (tv) {
    case 0:  for (i = 0; i < 3; ++i) T[i] = (int64_t)g->TR[i] * 4096; break;
    case 1:  for (i = 0; i < 3; ++i) T[i] = (int64_t)g->BK[i] * 4096; break;
    case 2:  for (i = 0; i < 3; ++i) T[i] = (int64_t)g->FC[i] * 4096; break;
    default: for (i = 0; i < 3; ++i) T[i] = 0; break;
    }

    for (row = 0; row < 3; ++row) {
        const int n = row + 1;
        if (tv == 2) {
            /* FC translation: the X term only feeds a temporary IR saturation
             * (hardware wiring); MAC/IR come from the Y/Z dot product. */
            int64_t first = gte_mac44(g, T[row] + (int64_t)M[row][0] * V[0], n);
            int32_t temporary = (int32_t)(first >> shift);
            int64_t tail;
            g->IR[n] = gte_sat_ir(g, temporary, n, 0);
            tail = gte_mac44(g, (int64_t)M[row][1] * V[1], n);
            gte_store_mac_ir(g, n, tail + (int64_t)M[row][2] * V[2], shift, lm);
        } else {
            int64_t acc = gte_mac44(g, T[row] + (int64_t)M[row][0] * V[0], n);
            acc = gte_mac44(g, acc + (int64_t)M[row][1] * V[1], n);
            gte_store_mac_ir(g, n, acc + (int64_t)M[row][2] * V[2], shift, lm);
        }
    }

    gte_set_error(g);
}

static void gte_op(gte_core *g, uint32_t instr) {
    const int sf = (instr >> 19) & 1;
    const int lm = (instr >> 10) & 1;
    const int64_t d1 = g->RT[0][0], d2 = g->RT[1][1], d3 = g->RT[2][2];
    int64_t m1 = d2 * g->IR[3] - d3 * g->IR[2];
    int64_t m2 = d3 * g->IR[1] - d1 * g->IR[3];
    int64_t m3 = d1 * g->IR[2] - d2 * g->IR[1];
    g->FLAG = 0;
    if (sf) { m1 >>= 12; m2 >>= 12; m3 >>= 12; }
    g->MAC[1] = (int32_t)m1;
    g->MAC[2] = (int32_t)m2;
    g->MAC[3] = (int32_t)m3;
    g->IR[1] = gte_sat_ir(g, g->MAC[1], 1, lm);
    g->IR[2] = gte_sat_ir(g, g->MAC[2], 2, lm);
    g->IR[3] = gte_sat_ir(g, g->MAC[3], 3, lm);
    gte_set_error(g);
}

static void gte_sqr(gte_core *g, uint32_t instr) {
    const int sf = (instr >> 19) & 1;
    const int lm = (instr >> 10) & 1;
    int64_t m1 = (int64_t)g->IR[1] * g->IR[1];
    int64_t m2 = (int64_t)g->IR[2] * g->IR[2];
    int64_t m3 = (int64_t)g->IR[3] * g->IR[3];
    g->FLAG = 0;
    if (sf) { m1 >>= 12; m2 >>= 12; m3 >>= 12; }
    g->MAC[1] = (int32_t)m1;
    g->MAC[2] = (int32_t)m2;
    g->MAC[3] = (int32_t)m3;
    g->IR[1] = gte_sat_ir(g, g->MAC[1], 1, lm);
    g->IR[2] = gte_sat_ir(g, g->MAC[2], 2, lm);
    g->IR[3] = gte_sat_ir(g, g->MAC[3], 3, lm);
    gte_set_error(g);
}

static void gte_gpf(gte_core *g, uint32_t instr) {
    const int sf = (instr >> 19) & 1;
    const int lm = (instr >> 10) & 1;
    int64_t m1 = (int64_t)g->IR[0] * g->IR[1];
    int64_t m2 = (int64_t)g->IR[0] * g->IR[2];
    int64_t m3 = (int64_t)g->IR[0] * g->IR[3];
    g->FLAG = 0;
    if (sf) { m1 >>= 12; m2 >>= 12; m3 >>= 12; }
    g->MAC[1] = (int32_t)m1;
    g->MAC[2] = (int32_t)m2;
    g->MAC[3] = (int32_t)m3;
    g->IR[1] = gte_sat_ir(g, g->MAC[1], 1, lm);
    g->IR[2] = gte_sat_ir(g, g->MAC[2], 2, lm);
    g->IR[3] = gte_sat_ir(g, g->MAC[3], 3, lm);
    gte_push_rgb_from_mac(g);
    gte_set_error(g);
}

static void gte_gpl(gte_core *g, uint32_t instr) {
    const int sf = (instr >> 19) & 1;
    const int lm = (instr >> 10) & 1;
    int64_t m1 = (sf ? (int64_t)g->MAC[1] * 4096 : (int64_t)g->MAC[1]) + (int64_t)g->IR[0] * g->IR[1];
    int64_t m2 = (sf ? (int64_t)g->MAC[2] * 4096 : (int64_t)g->MAC[2]) + (int64_t)g->IR[0] * g->IR[2];
    int64_t m3 = (sf ? (int64_t)g->MAC[3] * 4096 : (int64_t)g->MAC[3]) + (int64_t)g->IR[0] * g->IR[3];
    g->FLAG = 0;
    if (sf) { m1 >>= 12; m2 >>= 12; m3 >>= 12; }
    g->MAC[1] = (int32_t)m1;
    g->MAC[2] = (int32_t)m2;
    g->MAC[3] = (int32_t)m3;
    g->IR[1] = gte_sat_ir(g, g->MAC[1], 1, lm);
    g->IR[2] = gte_sat_ir(g, g->MAC[2], 2, lm);
    g->IR[3] = gte_sat_ir(g, g->MAC[3], 3, lm);
    gte_push_rgb_from_mac(g);
    gte_set_error(g);
}

/* ------------------------------------------------------------------------- */
/* Entry point                                                               */
/* ------------------------------------------------------------------------- */

void plat_gte_command(plat_gte_state *s, uint32_t command) {
    gte_core g;
    const uint32_t op = command & 0x3Fu;
    const int sf = (command >> 19) & 1;
    const int shift = sf ? 12 : 0;
    const int lm = (command >> 10) & 1;

    switch (op) {
    case 0x01: case 0x06: case 0x0C: case 0x10: case 0x11: case 0x12:
    case 0x13: case 0x14: case 0x16: case 0x1B: case 0x1C: case 0x1E:
    case 0x20: case 0x28: case 0x29: case 0x2A: case 0x2D: case 0x2E:
    case 0x30: case 0x3D: case 0x3E: case 0x3F:
        break;
    default:
        return;   /* unknown opcode: no effect */
    }

    gte_import(&g, s);

    switch (op) {
    case 0x01: gte_rtps(&g, shift, lm); break;
    case 0x30: gte_rtpt(&g, shift, lm); break;
    case 0x06: gte_nclip(&g); break;
    case 0x2D: gte_avsz3(&g); break;
    case 0x2E: gte_avsz4(&g); break;
    case 0x0C: gte_op(&g, command); break;
    case 0x28: gte_sqr(&g, command); break;
    case 0x3D: gte_gpf(&g, command); break;
    case 0x3E: gte_gpl(&g, command); break;
    case 0x10: gte_dpcs(&g, shift, lm); break;
    case 0x2A: gte_dpct(&g, shift, lm); break;
    case 0x29: gte_dpcl(&g, shift, lm); break;
    case 0x11: gte_intpl(&g, shift, lm); break;
    case 0x14: gte_cdp(&g, shift, lm); break;
    case 0x1C: gte_cc(&g, shift, lm); break;
    case 0x12: gte_mvmva(&g, command, shift, lm); break;
    case 0x1B: gte_nccs_n(&g, 1, shift, lm); break;
    case 0x3F: gte_nccs_n(&g, 3, shift, lm); break;
    case 0x1E: gte_ncs_n(&g, 1, shift, lm); break;
    case 0x20: gte_ncs_n(&g, 3, shift, lm); break;
    case 0x13: gte_ncds_n(&g, 1, shift, lm); break;
    case 0x16: gte_ncds_n(&g, 3, shift, lm); break;
    default: break;
    }

    gte_export(s, &g);
}
