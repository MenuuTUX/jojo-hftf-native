#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_80012538(const GuestMem *m, uint32_t a0, uint32_t a1);
uint32_t man_800121F0(const GuestMem *m, uint32_t a0, uint32_t a1);
uint32_t man_80012048(const GuestMem *m, uint32_t a0, uint32_t a1);

/* 80031C0C (TEXT): menu screen builder. Calls the text helpers with a 16-bit record at sp+0x10 (x, y, count, flags)
 * and a halfword at sp+0x18; both live in guest memory because the helpers read them by address. The record is
 * written exactly as the MIPS stores it, and every call sees the same memory state. $a2 and $a3 are passed on
 * unchanged by the original's first call (to 80012538, which does not read them). */
uint32_t man_80031C0C(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx) - 0x48u;   /* 0x48 = the original "addiu sp,sp,-72" */
    uint32_t p18 = sp + 0x18u;                 /* $s0 in the first block of calls */
    uint32_t s7 = 0x8006CE30u;                 /* first table pointer (halfwords) */
    uint32_t fp = 0x8006CE40u;                 /* second table pointer (halfwords) */
    uint32_t s6;
    uint32_t ent;
    uint32_t str;
    uint32_t v;
    uint32_t v3;
    uint32_t r;
    uint32_t a1v;

    m->sp_set(m->ctx, sp);

    /* 80031C24..80031C70 */
    WR16(sp + 0x10u, 132u);
    WR16(sp + 0x12u, 210u);
    WR16(sp + 0x14u, 11u);
    WR8(sp + 0x16u, 0u);
    WR8(sp + 0x17u, 0u);
    man_80012538(m, sp + 0x10u, 0x80054F4Cu);

    /* 80031C74..80031CB8 */
    WR16(sp + 0x10u, 16u);
    WR16(sp + 0x12u, 184u);
    a2 = 5u;
    WR16(sp + 0x14u, 5u);
    WR8(sp + 0x16u, 0u);
    WR8(sp + 0x17u, 0u);
    man_800121F0(m, sp + 0x10u, 0x80054F58u);
    WR16(sp + 0x10u, 336u);
    man_800121F0(m, sp + 0x10u, 0x80054F5Cu);

    /* 80031CBC..80031D00 */
    WR16(sp + 0x10u, 40u);
    WR16(sp + 0x12u, 154u);
    WR16(sp + 0x14u, 2u);
    WR8(sp + 0x16u, 10u);
    WR8(sp + 0x17u, 0u);
    WR16(sp + 0x18u, 0xE090u);
    man_80012538(m, sp + 0x10u, p18);
    WR16(sp + 0x10u, 200u);
    man_80012538(m, sp + 0x10u, p18);

    /* 80031D0C..80031D4C: first halfword at sp+0x18 is advanced by 12 */
    s6 = 0u;
    v3 = RD16(sp + 0x18u);
    WR16(sp + 0x10u, 40u);
    WR16(sp + 0x12u, 138u);
    WR16(sp + 0x14u, a2);
    WR16(sp + 0x18u, v3 + 12u);
    man_80012538(m, sp + 0x10u, p18);
    WR16(sp + 0x10u, 200u);
    man_80012538(m, sp + 0x10u, p18);

    /* 80031D50..80031D88 */
    WR16(sp + 0x12u, 186u);
    v = RD16(sp + 0x18u);
    WR16(sp + 0x10u, 40u);
    WR16(sp + 0x14u, 1u);
    WR16(sp + 0x18u, v + 12u);
    man_80012538(m, sp + 0x10u, p18);
    WR16(sp + 0x10u, 200u);
    man_80012538(m, sp + 0x10u, p18);

    /* 80031D94..80031DC4 */
    WR16(sp + 0x12u, 170u);
    v = RD16(sp + 0x18u);
    WR16(sp + 0x10u, 40u);
    WR16(sp + 0x14u, 4u);
    WR16(sp + 0x18u, v + 12u);
    man_80012538(m, sp + 0x10u, p18);
    WR16(sp + 0x10u, 200u);
    man_80012538(m, sp + 0x10u, p18);

    /* 80031DD0..80031E00 */
    WR16(sp + 0x12u, 122u);
    v = RD16(sp + 0x18u);
    WR16(sp + 0x10u, 40u);
    WR16(sp + 0x14u, 9u);
    WR16(sp + 0x18u, v + 12u);
    man_80012538(m, sp + 0x10u, p18);
    WR16(sp + 0x10u, 200u);
    man_80012538(m, sp + 0x10u, p18);

    /* 80031E0C..80031E34 */
    v = RD16(sp + 0x18u);
    WR16(sp + 0x10u, 52u);
    WR16(sp + 0x18u, v + 12u);
    man_80012538(m, sp + 0x10u, p18);
    WR16(sp + 0x10u, 212u);
    man_80012538(m, sp + 0x10u, p18);

    /* 80031E40..80031E68 */
    v = RD16(sp + 0x18u);
    WR16(sp + 0x10u, 40u);
    WR16(sp + 0x12u, 106u);
    WR16(sp + 0x18u, v + 12u);
    man_80012538(m, sp + 0x10u, p18);
    WR16(sp + 0x10u, 200u);
    man_80012538(m, sp + 0x10u, p18);

    /* 80031E6C..80031E94 */
    v = RD16(sp + 0x18u);
    WR16(sp + 0x10u, 52u);
    WR16(sp + 0x18u, v + 12u);
    man_80012538(m, sp + 0x10u, p18);
    WR16(sp + 0x10u, 212u);
    man_80012538(m, sp + 0x10u, p18);

    /* 80031E9C..80031EC4 */
    WR16(sp + 0x12u, 90u);
    WR16(sp + 0x10u, 40u);
    WR16(sp + 0x18u, 0xF080u);
    man_80012538(m, sp + 0x10u, p18);
    WR16(sp + 0x10u, 200u);
    man_80012538(m, sp + 0x10u, p18);

    /* 80031EC8..80031EF0 */
    v = RD16(sp + 0x18u);
    WR16(sp + 0x10u, 52u);
    WR16(sp + 0x18u, v + 12u);
    man_80012538(m, sp + 0x10u, p18);
    WR16(sp + 0x10u, 212u);
    man_80012538(m, sp + 0x10u, p18);

    /* 80031EF4..80031F24 */
    v = RD16(sp + 0x18u);
    WR16(sp + 0x10u, 40u);
    WR16(sp + 0x12u, 74u);
    WR16(sp + 0x18u, v + 12u);
    man_80012538(m, sp + 0x10u, p18);
    WR16(sp + 0x10u, 200u);
    man_80012538(m, sp + 0x10u, p18);

    /* 80031F28..80031F50 */
    v = RD16(sp + 0x18u);
    WR16(sp + 0x10u, 52u);
    WR16(sp + 0x18u, v + 12u);
    man_80012538(m, sp + 0x10u, p18);
    WR16(sp + 0x10u, 212u);
    man_80012538(m, sp + 0x10u, p18);

    /* 80031F54..80031F90: menu labels through 800121F0 */
    str = 0x80054F24u;
    WR16(sp + 0x10u, 40u);
    WR16(sp + 0x12u, 56u);
    WR16(sp + 0x14u, a2);
    WR8(sp + 0x16u, 0u);
    WR8(sp + 0x17u, 0u);
    man_800121F0(m, sp + 0x10u, str);
    WR16(sp + 0x10u, 200u);
    man_800121F0(m, sp + 0x10u, str);

    /* 80031F94..80031FEC */
    WR16(sp + 0x10u, 40u);
    WR16(sp + 0x12u, 40u);
    man_800121F0(m, sp + 0x10u, 0x80054F60u);
    WR16(sp + 0x10u, 200u);
    man_800121F0(m, sp + 0x10u, 0x80054F60u);
    WR16(sp + 0x10u, 40u);
    WR16(sp + 0x12u, 24u);
    man_800121F0(m, sp + 0x10u, 0x80054F68u);
    WR16(sp + 0x10u, 200u);
    man_800121F0(m, sp + 0x10u, 0x80054F68u);

    /* 80031FF0..80032014: setup of the two per-slot value loops */
    WR16(sp + 0x10u, 78u);
    WR16(sp + 0x12u, 180u);
    WR8(sp + 0x16u, 0u);
    WR8(sp + 0x17u, 0u);

    /* 80032018..800320B8: eight rows; row i reads the i-th entry of each table */
    s6 = 0u;
    do {
        a2 = 5u;
        ent = 0x800959D0u + s6;                       /* $s0 = $s2 + i */
        WR16(sp + 0x14u, a2);
        if (RD8(ent) != RD16(s7)) {
            WR16(sp + 0x14u, 7u);
        }
        a1v = RD32(0x8005B9C0u + (RD16(s7) << 2));
        man_80012048(m, sp + 0x10u, a1v);

        WR16(sp + 0x10u, 238u);
        WR16(sp + 0x14u, 5u);
        v = RD16(fp);
        if (RD8(ent + 10u) != v) {
            WR16(sp + 0x14u, 7u);
        }
        s7 += 2u;
        fp += 2u;
        a1v = RD32(0x8005B9C0u + (v << 2));
        man_80012048(m, sp + 0x10u, a1v);
        s6 += 1u;

        v3 = RD16(sp + 0x12u);
        WR16(sp + 0x10u, 78u);
        v3 = v3 - 16u;
        WR16(sp + 0x12u, v3);
    } while (s6 != 8u);

    /* 800320BC..80032130: the first on/off line, selected by flag bit 0 of byte 0x80086CE57 */
    WR16(sp + 0x10u, 128u);
    WR16(sp + 0x12u, 52u);
    WR16(sp + 0x14u, 5u);
    if ((RD8(0x8006CE57u) & 1u) != 0u) {
        if (RD8(0x800959D8u) == 0u) {
            WR16(sp + 0x14u, 7u);
        }
        str = 0x80054F2Cu;
    } else {
        if (RD8(0x800959D8u) != 0u) {
            WR16(sp + 0x14u, 7u);
        }
        str = 0x80054F70u;
    }
    man_80012048(m, sp + 0x10u, str);

    /* 80032138..800321A4: the second on/off line, selected by flag bit 1 */
    WR16(sp + 0x10u, 288u);
    WR16(sp + 0x14u, 5u);
    if ((RD8(0x8006CE57u) & 2u) != 0u) {
        if (RD8(0x800959E2u) == 0u) {
            WR16(sp + 0x14u, 7u);
        }
        str = 0x80054F2Cu;
    } else {
        if (RD8(0x800959E2u) != 0u) {
            WR16(sp + 0x14u, 7u);
        }
        str = 0x80054F70u;
    }
    r = man_80012048(m, sp + 0x10u, str);

    m->sp_set(m->ctx, sp + 0x48u);
    return r;
}
