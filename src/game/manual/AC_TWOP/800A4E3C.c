#include "manual.h"
#include "hex2c_rt.h"

/* AC_TWOP 800A4E3C..800A4EF8 (188 bytes). Leaf: no stack frame, no calls, no argument registers read.
 * Identical in runs/tables/ram_training.bin and ram_vs.bin. Hex-Rays' flat version folds the halfword compare
 * and the byte store; this follows the MIPS branch by branch.
 *
 * Data: byte 0x80066CAD (CAD), byte 0x80066CAE (CAE), byte 0x80066CAF (SEL, written here),
 * halfwords 0x800CDEBA (A) and 0x800CE24A (B), bytes 0x800CDD55 (P) and 0x800CE0E5 (Q),
 * table 0x800D0880 with stride 68, count byte at +0x23 of the selected entry.
 *
 * Returns the address of the selected table entry, 2 for the early "equal halfword" exit, or 0 for the
 * "CAE == 0" exit. The delay slot of the beq at 800a4e78 sets $v0 = 2 before the branch, so the equal exit returns 2. */
uint32_t man_AC_TWOP_800A4E3C(const GuestMem *m) {
    uint32_t v0, sel, addr, cnt;
    uint8_t b5;

    v0 = RD8(0x80066CADu);                          /* lbu v0,0xd(a2) */
    if (v0 != 0u) {                                 /* bne-not-taken path: beq at 800a4e54 falls through */
        v0 = RD8(0x80066CAEu);                      /* lbu v0,0xe(a2) */
        if (v0 == 0u) {                             /* beq v0,zero,800a4ef0: return v0 = 0 */
            return 0u;
        }
    }

    /* 800a4e6c */
    if ((int32_t)RDS16(0x800CDEBAu) == (int32_t)RDS16(0x800CE24Au)) {
        return 2u;                                  /* beq v1,v0,800a4ef0; delay slot addiu v0,zero,2 ran first */
    }

    b5 = RD8(0x800CDD55u);                          /* lbu v1,0x5(a0): P */
    if (b5 == 2u && RD8(0x800CE0E5u) == 2u) {       /* bne v1,v0 (v0 = 2) not taken, beq v0,v1 taken: byte = 0 */
        sel = 0u;
    } else {
        /* 800a4ea0: slt v0,B,A (signed); beq v0,zero taken when B >= A, delay slot sets v0 = 1 */
        sel = ((int32_t)RDS16(0x800CE24Au) < (int32_t)RDS16(0x800CDEBAu)) ? 0u : 1u;
    }
    WR8(0x80066CAFu, sel);                          /* sb v0,0xf(a2) / sb zero,0xf(a2) */

    /* 800a4ec4: entry = 0x800D0880 + 68 * SEL */
    sel = RD8(0x80066CAFu);                         /* lbu a0,0xf(a2) */
    addr = 0x800D0880u + sel * 68u;                 /* (a0<<4)+a0, <<2, + 0x800D0880 */
    v0 = addr;
    cnt = RD8(addr + 0x23u);                        /* lbu v1,0x23(v0) */
    WR8(addr + 0x23u, cnt + 1u);                    /* addiu v1,v1,1; sb v1,0x23(v0) */

    return v0;                                      /* 800a4ef0: jr ra with v0 = addr */
}
