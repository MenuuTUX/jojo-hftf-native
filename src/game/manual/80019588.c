#include "manual.h"
#include "hex2c_rt.h"

/* 80019588 (TEXT): uploads the per-character sprite sheets. The RECT is the global 0x800797B0 (x, y, w, h halfwords), the
 * LoadImage pointers are dword_80079618[i], the sizes are dword_800796A0[i], where i = ((sext(byte 0x136) << 3) | 3) & 0xFF.
 * Ten calls, in order: LoadImage(0x80036348) then 80027414(2, RECT, ptr) for each pair, indices i, i+1, i+4, i+2, i+3.
 * Bytes 0x136 and 0x214 of the argument are re-read before each use, as the original does. Only saved registers are used,
 * so the frame needs no guest memory. Returns the $v0 of the last 80027414 call. */
uint32_t man_80019588(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2) {
    uint32_t i, j, s0, s3, p;
    (void)a1;

    i = (((uint32_t)RDS8(a0 + 0x136u) << 3) | 3u) & 0xFFu;

    /* pass 1: index i */
    WR16(0x800797B0u, 0);
    WR16(0x800797B4u, 128u);
    WR16(0x800797B6u, 1u);
    WR16(0x800797B2u, (uint32_t)RDS8(a0 + 0x136u) + 488u);
    s0 = 0x80079618u + i * 4u;
    m->call4(m->ctx, 0x80036348u, 0x800797B0u, RD32(s0) + ((uint32_t)RD8(a0 + 0x214u) << 8), a2, 128u);
    m->call4(m->ctx, 0x80027414u, 2u, 0x800797B0u, RD32(s0) + ((uint32_t)RD8(a0 + 0x214u) << 8), 0u);

    /* pass 2: index i+1, size from dword_800796A0[i+1] */
    s3 = 0x800796A0u + (i + 1u) * 4u;
    WR16(0x800797B2u, (uint32_t)RDS8(a0 + 0x136u) + 495u);
    WR16(0x800797B6u, 1u);
    WR16(0x800797B4u, RD32(s3) >> 2);
    p = (RD32(s3) >> 1) * (uint32_t)RD8(a0 + 0x214u);
    s0 = 0x80079618u + (i + 1u) * 4u;
    m->call4(m->ctx, 0x80036348u, 0x800797B0u, RD32(s0) + p, 0u, p);
    p = (RD32(s3) >> 1) * (uint32_t)RD8(a0 + 0x214u);
    m->call4(m->ctx, 0x80027414u, 2u, 0x800797B0u, RD32(s0) + p, p);

    /* pass 3: index i+4, fixed w = 384, h = 2, y = 2 * sext(byte) + 499 */
    WR16(0x800797B4u, 384u);
    WR16(0x800797B6u, 2u);
    s0 = 0x80079618u + (i + 4u) * 4u;
    WR16(0x800797B2u, ((uint32_t)RDS8(a0 + 0x136u) << 1) + 499u);
    m->call4(m->ctx, 0x80036348u, 0x800797B0u, RD32(s0), 0u, 0u);
    m->call4(m->ctx, 0x80027414u, 2u, 0x800797B0u, RD32(s0), 0u);

    /* pass 4: index i+2, x = 128 */
    WR16(0x800797B0u, 128u);
    s3 = 0x800796A0u + (i + 2u) * 4u;
    WR16(0x800797B2u, (uint32_t)RDS8(a0 + 0x136u) + 488u);
    WR16(0x800797B6u, 1u);
    WR16(0x800797B4u, RD32(s3) >> 2);
    p = (RD32(s3) >> 1) * (uint32_t)RD8(a0 + 0x214u);
    s0 = 0x80079618u + (i + 2u) * 4u;
    m->call4(m->ctx, 0x80036348u, 0x800797B0u, RD32(s0) + p, 0u, p);
    p = (RD32(s3) >> 1) * (uint32_t)RD8(a0 + 0x214u);
    m->call4(m->ctx, 0x80027414u, 2u, 0x800797B0u, RD32(s0) + p, p);

    /* pass 5: index i+3, x = 0 */
    j = i + 3u;
    WR16(0x800797B0u, 0);
    s3 = 0x800796A0u + j * 4u;
    WR16(0x800797B2u, (uint32_t)RDS8(a0 + 0x136u) + 497u);
    WR16(0x800797B6u, 1u);
    WR16(0x800797B4u, RD32(s3) >> 2);
    p = (RD32(s3) >> 1) * (uint32_t)RD8(a0 + 0x214u);
    s0 = 0x80079618u + j * 4u;
    m->call4(m->ctx, 0x80036348u, 0x800797B0u, RD32(s0) + p, 0u, p);
    p = (RD32(s3) >> 1) * (uint32_t)RD8(a0 + 0x214u);
    return m->call4(m->ctx, 0x80027414u, 2u, 0x800797B0u, RD32(s0) + p, p);
}
