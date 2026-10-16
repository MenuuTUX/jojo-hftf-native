#include "plat_math.h"

#include <string.h>

static uint32_t sar32(uint32_t x, unsigned n) {
    if (n == 0) return x;
    if (n >= 32) return (x & 0x80000000u) ? UINT32_MAX : 0;
    return (x >> n) | ((x & 0x80000000u) ? (UINT32_MAX << (32 - n)) : 0);
}

static uint64_t sar64(uint64_t x, unsigned n) {
    if (n == 0) return x;
    if (n >= 64) return (x & UINT64_C(0x8000000000000000)) ? UINT64_MAX : 0;
    return (x >> n) | ((x & UINT64_C(0x8000000000000000))
        ? (UINT64_MAX << (64 - n)) : 0);
}

static uint32_t f32_inf(uint32_t sign) {
    return (sign & 0x80000000u) | 0x7f800000u;
}

uint32_t plat_sf_add_f32(uint32_t a, uint32_t b) {
    uint32_t sa, sb, sum, sign = 0;
    int ea, eb;

    /* These zero and large-exponent shortcuts are part of the original helper. */
    if ((a & 0x7fffffffu) == 0) return b;
    if ((b & 0x7fffffffu) == 0) return a;
    ea = (int)((a >> 23) & 0xffu);
    eb = (int)((b >> 23) & 0xffu);
    if (ea > eb + 25) return a;
    if (eb > ea + 25) return b;

    sa = ((a & 0x007fffffu) | 0x00800000u) << 6;
    sb = ((b & 0x007fffffu) | 0x00800000u) << 6;
    if (a & 0x80000000u) sa = 0u - sa;
    if (b & 0x80000000u) sb = 0u - sb;
    if (ea < eb) {
        sa = sar32(sa, (unsigned)(eb - ea));
        ea = eb;
    } else if (eb < ea) {
        sb = sar32(sb, (unsigned)(ea - eb));
    }
    sum = sa + sb;
    if (sum == 0) return 0;
    if (sum & 0x80000000u) { sign = 0x80000000u; sum = 0u - sum; }

    while ((sum & 0xe0000000u) == 0) { sum <<= 1; --ea; }
    if (sum & 0x40000000u) { sum = sar32(sum, 1); ++ea; }
    sum += (sum & 0x40u) ? 0x20u : 0x1fu;
    if (sum & 0x40000000u) { sum = sar32(sum, 1); ++ea; }
    if (ea < 0 || ea >= 255) return f32_inf(sign);
    return sign | ((uint32_t)ea << 23) | ((sum >> 6) & 0x007fffffu);
}

uint32_t plat_sf_ne_f32(uint32_t a, uint32_t b) {
    if (a == b) return 0;
    return ((a & 0x7fffffffu) || (b & 0x7fffffffu)) ? 1u : 0u;
}

int32_t plat_sf_f64_to_i32(uint32_t lo, uint32_t hi) {
    uint32_t sign = hi & 0x80000000u;
    unsigned exp = (hi >> 20) & 0x7ffu;
    uint64_t bits = ((uint64_t)hi << 32) | lo;
    uint64_t sig;
    unsigned shift;
    uint32_t out;

    if ((hi & 0x7fffffffu) == 0 && lo == 0) return 0;
    if (exp < 1023u) return 0;
    if (exp >= 1054u) return sign ? INT32_MIN : INT32_MAX;
    sig = (bits & UINT64_C(0x000fffffffffffff)) | UINT64_C(0x0010000000000000);
    shift = 1075u - exp;
    out = (uint32_t)(sig >> shift);
    return sign ? (int32_t)(0u - out) : (int32_t)out;
}

int32_t plat_sf_f32_to_i32(uint32_t bits) {
    uint32_t sign = bits & 0x80000000u;
    unsigned exp = (bits >> 23) & 0xffu;
    uint32_t sig;
    unsigned shift;
    if ((bits & 0x7fffffffu) == 0) return 0;
    if (exp < 127u) return 0;
    if (exp > 157u) return sign ? INT32_MIN : INT32_MAX;
    sig = (bits & 0x007fffffu) | 0x00800000u;
    shift = exp > 150u ? 0u : 150u - exp;
    if (exp > 150u) sig <<= exp - 150u;
    else sig >>= shift;
    return sign ? (int32_t)(0u - sig) : (int32_t)sig;
}

void plat_sf_i32_to_f64(uint32_t bits, uint32_t *lo, uint32_t *hi) {
    int32_t value;
    double d;
    uint64_t out;
    memcpy(&value, &bits, sizeof value);
    d = (double)value;
    memcpy(&out, &d, sizeof out);
    if (lo) *lo = (uint32_t)out;
    if (hi) *hi = (uint32_t)(out >> 32);
}

uint32_t plat_sf_i32_to_f32(uint32_t bits) {
    uint32_t sign = 0, mag = bits;
    int exp = 157;
    if (mag == 0) return 0;
    if (mag & 0x80000000u) { sign = 0x80000000u; mag = 0u - mag; }
    if (mag == 0x80000000u) return 0xcf000000u;
    if (mag <= 0x00ffffffu) {
        while (mag <= 0x00ffffffu) { mag <<= 4; exp -= 4; }
    }
    while (mag <= 0x3fffffffu) { mag <<= 1; --exp; }
    mag += 0x40u;
    if (mag & 0x80000000u) { ++exp; mag >>= 8; }
    else mag >>= 7;
    return sign | ((uint32_t)exp << 23) | (mag & 0x007fffffu);
}

uint32_t plat_sf_mul_f32(uint32_t a, uint32_t b) {
    uint32_t ma, mb, ah, bh, acc, p, sign;
    unsigned ea, eb;
    int exp;

    sign = (a ^ b) & 0x80000000u;
    ma = a & 0x007fffffu;
    mb = b & 0x007fffffu;
    if ((a & 0x7fffffffu) == 0 || (b & 0x7fffffffu) == 0) return sign;
    ma |= 0x00800000u;
    mb |= 0x00800000u;
    ah = ma >> 8; bh = mb >> 8;
    acc = ah * bh;
    p = (ma & 0xffu) * bh;
    acc += sar32(p, 8);
    p = (mb & 0xffu) * ah;
    acc += sar32(p, 8);
    ea = (a >> 23) & 0xffu;
    eb = (b >> 23) & 0xffu;
    exp = (int)ea + (int)eb;

    if (acc & 0x80000000u) {
        acc += 0x80u;
        acc >>= 8;
        exp -= 126;
    } else {
        acc += 0x40u;
        if (acc & 0x80000000u) { acc >>= 8; exp -= 126; }
        else { acc >>= 7; exp -= 127; }
    }
    if (exp >= 255) return f32_inf(sign);
    return sign | ((uint32_t)exp << 23) | (acc & 0x007fffffu);
}

static uint64_t add_f64(uint64_t a, uint64_t b) {
    const uint64_t abs_mask = UINT64_C(0x7fffffffffffffff);
    int ea, eb;
    uint64_t sa, sb, sum, sign = 0;
    if ((a & abs_mask) == 0) return b;
    if ((b & abs_mask) == 0) return a;
    ea = (int)((a >> 52) & 0x7ffu);
    eb = (int)((b >> 52) & 0x7ffu);
    if (ea > eb + 54) return a;
    if (eb > ea + 54) return b;

    sa = ((a & UINT64_C(0x000fffffffffffff)) | UINT64_C(0x0010000000000000)) << 9;
    sb = ((b & UINT64_C(0x000fffffffffffff)) | UINT64_C(0x0010000000000000)) << 9;
    if (a & UINT64_C(0x8000000000000000)) sa = 0 - sa;
    if (b & UINT64_C(0x8000000000000000)) sb = 0 - sb;
    if (ea < eb) { sa = sar64(sa, (unsigned)(eb - ea)); ea = eb; }
    else if (eb < ea) sb = sar64(sb, (unsigned)(ea - eb));
    sum = sa + sb;
    if (sum == 0) return 0;
    if (sum & UINT64_C(0x8000000000000000)) {
        sign = UINT64_C(0x8000000000000000);
        sum = 0 - sum;
    }
    while ((((uint32_t)(sum >> 32)) & 0xe0000000u) == 0) { sum <<= 1; --ea; }
    if (((uint32_t)(sum >> 32)) & 0x40000000u) { sum = sar64(sum, 1); ++ea; }
    sum += (((uint32_t)sum & 0x200u) ? 0x100u : 0xffu);
    if ((((uint32_t)(sum >> 32)) & 0xe0000000u) == 0) {
        while ((((uint32_t)(sum >> 32)) & 0xe0000000u) == 0) { sum <<= 1; --ea; }
    }
    if (((uint32_t)(sum >> 32)) & 0x40000000u) { sum = sar64(sum, 1); ++ea; }
    sum = sar64(sum, 9);
    if (ea >= 2047) return sign | UINT64_C(0x7ff0000000000000);
    return sign | ((uint64_t)(uint32_t)ea << 52) |
        (sum & UINT64_C(0x000fffffffffffff));
}

void plat_sf_sub_f64(uint32_t alo, uint32_t ahi, uint32_t blo, uint32_t bhi,
                     uint32_t *lo, uint32_t *hi) {
    uint64_t a = ((uint64_t)ahi << 32) | alo;
    uint64_t b = ((uint64_t)bhi << 32) | blo;
    uint64_t out;
    if ((b & UINT64_C(0x7fffffffffffffff)) == 0) out = a;
    else if ((a & UINT64_C(0x7fffffffffffffff)) == 0)
        out = b ^ UINT64_C(0x8000000000000000);
    else out = add_f64(a, b ^ UINT64_C(0x8000000000000000));
    if (lo) *lo = (uint32_t)out;
    if (hi) *hi = (uint32_t)(out >> 32);
}

void plat_sf_fragment_dc00(uint32_t v0, uint32_t *out_v0, uint32_t *out_v1) {
    if (out_v0) *out_v0 = 0;
    if (out_v1) *out_v1 = v0;
}

uint32_t plat_sf_sub_f32(uint32_t a, uint32_t b) {
    /* ddc0 tests the original right operand for zero before negating it. */
    if ((b & 0x7fffffffu) == 0) return a;
    return plat_sf_add_f32(a, b ^ 0x80000000u);
}

void *plat_bcopy(const void *src, void *dst, size_t n) {
    /* The original bcopy (0x80035c10) is a forward byte loop: overlapping ranges smear, which memmove does not do. */
    const uint8_t *s = (const uint8_t *)src;
    uint8_t *d = (uint8_t *)dst;
    while (n--) *d++ = *s++;
    return (void *)src;
}

void plat_bzero(void *dst, size_t n) { memset(dst, 0, n); }

void *plat_memset(void *dst, int value, size_t n) { return memset(dst, value, n); }
