#include "manual.h"
#include "hex2c_rt.h"

/* 800AE858 (AC_MAIN, 372 bytes), hand-written from ida/decomp/fight/AC_MAIN/800ae858.c and the MIPS.
 * Register mapping: pseudocode a1 = $s1 = incoming $a0 (object pointer); pseudocode a2 = incoming $a1 (value).
 * $a2 is set at entry to the incoming $a1 (800ae864) and is not written again, so every call sees it unchanged.
 * $a3 is set at entry to 0x80066CA0 (lui/addiu 800ae868..86c) and is not written again.
 * Hidden-argument calls:
 *  800ae8b0 jal 800A1B70 (early path: byte_80066C6C == 3 and the byte_80066C6A test is zero):
 *    a0 = object ($s1), a1 = (int16)value (sll/sra at 800ae8ac..8b4; the sra is the delay slot and runs before the callee),
 *    a2 = incoming value, a3 = 0x80066CA0. Its $v0 is returned.
 *  800ae960 jal 800AE9CC (else path): a0 = object ($s1, set at 800ae95c), a1 = (int16)masked value (sra at 800ae950),
 *    a2 = incoming value, a3 = 0x80066CA0. The jal delay slot (sh at 800ae964) runs before the callee.
 * Frame: addiu sp,-32 (800ae858) with the callee calls made under sp-32. The s0/s1/ra spills at sp+0x10..0x18 are not stored,
 * as in 8001E29C.
 * Return: $v0 as the MIPS leaves it: the callee $v0 on the early path, the byte at object+0x1FD on the then path (800ae90c),
 * and ~(uint32)halfword at s0+0x1DA on the else path (800ae988..990). */
uint32_t man_800AE858(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx);
    uint32_t s1 = a0;                 /* addu s1,a0,zero (800ae860) */
    uint32_t s0;                      /* lw s0,0x170(s1): v3 = *(_WORD **)(a1 + 368) */
    uint32_t a1r = a1;                /* $a1 working value (pseudocode a2) */
    uint32_t a2r = a1;                /* $a2 = incoming $a1 (800ae864); never written again */
    uint32_t a3r = 0x80066CA0u;       /* $a3 = byte_80066CA0 base (800ae86c); never written again */
    uint32_t v0, ret, t0, t1, t2;

    (void)a2;                         /* incoming $a2 and $a3 are overwritten at entry; see a2r and a3r */
    (void)a3;
    m->sp_set(m->ctx, sp - 32u);
    s0 = RD32(s1 + 0x170u);

    /* if (byte_80066C6C == 3 && (byte_80066C6A & (*(char *)(a1 + 310) + 1)) == 0) return unk_800A1B70(a1, (__int16)a2); */
    if (RD8(0x80066C6Cu) == 3u && (RD8(0x80066C6Au) & ((int32_t)RDS8(s1 + 0x136u) + 1)) == 0) {
        a1r = (uint32_t)(int32_t)(int16_t)(uint16_t)a2r;     /* sll a1,a2,16 / sra a1,a1,16 (800ae8ac..8b4) */
        ret = m->call4(m->ctx, 0x800A1B70u, s1, a1r, a2r, a3r);
        m->sp_set(m->ctx, sp);
        return ret;
    }

    /* if (BYTE1(dword_80066CA8) != 0 || (v5 = byte_80066CA0[*(char *)(a1 + 310) + 19]) == 2 || v5 == 3) a2 = 0; */
    if (RD8(0x80066CA9u) != 0u) {
        a1r = 0u;
    } else {
        v0 = RD8(0x80066CA0u + (uint32_t)((int32_t)RDS8(s1 + 0x136u) + 0x13));
        if (v0 == 2u || v0 == 3u) {
            a1r = 0u;
        }
    }

    /* if (byte_80066CA5[2] == 0) a2 &= 0xF140u; */
    if (RD8(0x80066CA7u) == 0u) {
        a1r &= 0xF140u;
    }

    a1r <<= 16;                       /* v6 = a2 << 16 (800ae918, delay slot of the bne below: runs on both paths) */
    v0 = RD8(s1 + 0x1FDu);            /* result = *(unsigned __int8 *)(a1 + 509) (800ae90c) */
    if (v0 != 0u) {
        /* then path: a1->482, 474, 472 and v3[241], v3[237], v3[236] are zeroed; $v0 keeps the byte read above */
        WR16(s1 + 0x1E2u, 0u);
        WR16(s1 + 0x1DAu, 0u);
        WR16(s1 + 0x1D8u, 0u);
        WR16(s0 + 0x1E2u, 0u);
        WR16(s0 + 0x1DAu, 0u);
        WR16(s0 + 0x1D8u, 0u);
        m->sp_set(m->ctx, sp);
        return v0;
    }

    /* else path: shift the history words of a1 and v3, as the MIPS does (loads first, then stores, per block) */
    t0 = RD16(s1 + 0x1DEu);
    t1 = RD16(s1 + 0x1DCu);
    t2 = RD16(s1 + 0x1DAu);
    WR16(s1 + 0x1E0u, t0);            /* a1->480 = a1->478 */
    WR16(s1 + 0x1DEu, t1);            /* a1->478 = a1->476 */
    WR16(s1 + 0x1DCu, t2);            /* a1->476 = a1->474 */

    t0 = RD16(s0 + 0x1DEu);
    t1 = RD16(s0 + 0x1DCu);
    t2 = RD16(s0 + 0x1DAu);
    WR16(s0 + 0x1E0u, t0);            /* v3[240] = v3[239] */
    WR16(s0 + 0x1DEu, t1);            /* v3[239] = v3[238] */
    WR16(s0 + 0x1DCu, t2);            /* v3[238] = v3[237] */

    WR16(s1 + 0x1DAu, RD16(s1 + 0x1D8u));   /* a1->474 = a1->472 (800ae94c..954) */
    a1r = (uint32_t)((int32_t)a1r >> 16);   /* sra a1,a1,16 (800ae950) */
    WR16(s0 + 0x1DAu, RD16(s0 + 0x1D8u));   /* v3[237] = v3[236] (800ae958..964; the sh is the jal delay slot) */
    m->call4(m->ctx, 0x800AE9CCu, s1, a1r, a2r, a3r);

    WR16(s0 + 0x1D8u, RD16(s1 + 0x1D8u));   /* v3[236] = a1->472 (800ae968..970) */
    t0 = RD16(s1 + 0x1DAu);
    t1 = RD16(s1 + 0x1D8u);
    WR16(s1 + 0x1E2u, t1 & ~t0);            /* a1->482 = a1->472 & ~a1->474 (800ae974..984) */

    t0 = RD16(s0 + 0x1DAu);
    t1 = RD16(s0 + 0x1D8u);
    ret = ~t0;                              /* result = ~(unsigned __int16)v3[237]; nor v0,zero,v0 (800ae988..990) */
    WR16(s0 + 0x1E2u, t1 & ~t0);            /* v3[241] = v3[236] & result (800ae98c..99c) */
    m->sp_set(m->ctx, sp);
    return ret;
}
