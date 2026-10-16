#include "manual.h"
#include "hex2c_rt.h"

/* 800C5808: builds the object links. Callees 800256B8(type, index) and 80025548(kind, size, slot) are not in the manual
 * queue, so they are called with m->call4. The zero passed as $a2/$a3 for 800256B8 and as $a3 for 80025548 is never read:
 * 800256B8 sets $a2 before reading it and 80025548 overwrites $a3 at entry.
 * The two stores of $s1 at 800C5BC8 and 800C5BDC have no null check in the MIPS (they sit in jal delay slots): a null
 * result writes to guest address 0x18, as the original does.
 * Returns 0 when the 800256B8(0,0) result is null (the early exit at 800C592C), otherwise 1 (the $v0 set in the delay
 * slot at 800C5C08). Uses no stack frame beyond the saved registers. */
uint32_t man_800C5808(const GuestMem *m) {
    uint32_t s0, s1, s9, v, r, i;
    int32_t c;

    /* object pair 0 */
    s1 = m->call4(m->ctx, 0x800256B8u, 1, 0, 0, 0);
    if (s1 != 0u) {
        WR16(s1 + 0x12u, (uint32_t)-88);
        WR8(s1 + 4u, 0);
        WR16(s1 + 0x16u, 16);
        for (s0 = 0; s0 < 2u; s0++) {
            v = m->call4(m->ctx, 0x80025548u, 8, 4, 1, 0);
            if (v != 0u) {
                WR8(v + 4u, s0);
                WR32(v + 0x80u, s1);
            }
        }
        v = m->call4(m->ctx, 0x800256B8u, 2, 0, 0, 0);
        if (v != 0u)
            WR32(v + 0x18u, s1);
    }

    /* object pair 1 */
    s1 = m->call4(m->ctx, 0x800256B8u, 1, 1, 0, 0);
    if (s1 != 0u) {
        WR8(s1 + 4u, 1);
        WR16(s1 + 0x12u, 456);
        WR16(s1 + 0x16u, 16);
        for (s0 = 2; s0 < 4u; s0++) {
            v = m->call4(m->ctx, 0x80025548u, 8, 4, 1, 0);
            if (v != 0u) {
                WR8(v + 4u, s0);
                WR32(v + 0x80u, s1);
            }
        }
        v = m->call4(m->ctx, 0x800256B8u, 2, 1, 0, 0);
        if (v != 0u)
            WR32(v + 0x18u, s1);
    }

    s1 = m->call4(m->ctx, 0x800256B8u, 0, 0, 0, 0);
    if (s1 == 0u)
        return 0u;

    v = m->call4(m->ctx, 0x800256B8u, 3, 0, 0, 0);
    if (v != 0u)
        WR32(v + 0x18u, s1);

    /* 0x80066C6C and 0x8006CE51 are signed bytes (lb) */
    c = RDS8(0x80066C6Cu);
    if (c == 0 || c == 5 || (RDS8(0x8006CE51u) != 3 && c != 3)) {
        v = m->call4(m->ctx, 0x800256B8u, 3, 1, 0, 0);
        if (v != 0u)
            WR32(v + 0x18u, s1);
    }

    v = m->call4(m->ctx, 0x800256B8u, 4, 0, 0, 0);
    if (v != 0u)
        WR32(v + 0x18u, s1);
    v = m->call4(m->ctx, 0x800256B8u, 4, 1, 0, 0);
    if (v != 0u)
        WR32(v + 0x18u, s1);

    /* dword_800CBEB0 is a byte table indexed by the signed byte at 0x800CDE87 (and 0x800CE217) */
    if (RDS8(0x800CBEB0u + (uint32_t)RDS8(0x800CDE87u)) != 0) {
        v = m->call4(m->ctx, 0x800256B8u, 5, 0, 0, 0);
        if (v != 0u)
            WR32(v + 0x18u, s1);
        v = m->call4(m->ctx, 0x800256B8u, 6, 0, 0, 0);
        if (v != 0u)
            WR32(v + 0x18u, s1);
    }

    if (RDS8(0x800CBEB0u + (uint32_t)RDS8(0x800CE217u)) != 0) {
        v = m->call4(m->ctx, 0x800256B8u, 5, 1, 0, 0);
        if (v != 0u)
            WR32(v + 0x18u, s1);
        v = m->call4(m->ctx, 0x800256B8u, 6, 1, 0, 0);
        if (v != 0u)
            WR32(v + 0x18u, s1);
    }

    /* 0x80066CA0 is a small table: byte 1 = count A, byte 6 = count D. The MIPS has no test for D == 255
     * (the pseudocode's "!= -1" is not in the disassembly: addiu/beq test D + 1 == 0, which never holds). */
    if (RD8(0x80066CA1u) == 0u || RDS8(0x80066C6Cu) == 0) {
        s0 = 0;
        do {
            v = m->call4(m->ctx, 0x800256B8u, 7, s0 & 0xFFu, 0, 0);
            if (v != 0u)
                WR32(v + 0x18u, s1);
            v = m->call4(m->ctx, 0x800256B8u, 7, (s0 | 4u) & 0xFFu, 0, 0);
            if (v != 0u)
                WR32(v + 0x18u, s1);
            s0++;
        } while ((s0 & 0xFFFFu) < RD8(0x80066CA6u) + 1u);
    }
    s0 = 0;

    for (s0 = 0; s0 < 4u; s0++) {
        v = m->call4(m->ctx, 0x80025548u, 8, 4, 0, 0);
        if (v != 0u) {
            WR32(v + 0x80u, s1);
            WR8(v + 4u, s0);
        }
    }

    /* 0x800CDD50 is a table of 228-dword rows (912 bytes); the byte at row offset 0x137 indexes dword_800CBEB0 */
    for (s0 = 0; s0 < 4u; s0++) {
        i = (s0 & 0xFFFFu) >> 1;
        if (RDS8(0x800CBEB0u + (uint32_t)RDS8(0x800CDD50u + 912u * i + 0x137u)) != 0) {
            v = m->call4(m->ctx, 0x80025548u, 8, 4, 2, 0);
            if (v != 0u) {
                WR32(v + 0x80u, s1);
                WR8(v + 4u, s0);
            }
        }
    }

    s9 = m->call4(m->ctx, 0x800256B8u, 9, 0, 0, 0);
    r = m->call4(m->ctx, 0x800256B8u, 8, 0, 0, 0);
    WR32(r + 0x18u, s9);                       /* delay slot of the next jal: stores into this $v0 */
    r = m->call4(m->ctx, 0x800256B8u, 8, 1, 0, 0);
    WR32(r + 0x18u, s9);                       /* delay slot of the next jal */
    r = m->call4(m->ctx, 0x80025548u, 8, 4, 3, 0);
    if (r != 0u) {
        WR8(r + 4u, 0);
        WR32(r + 0x80u, s9);
    }
    r = m->call4(m->ctx, 0x80025548u, 8, 4, 3, 0);
    if (r != 0u) {
        WR8(r + 4u, 1);
        WR32(r + 0x80u, s9);
    }
    return 1u;
}
