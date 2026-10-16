#include "manual.h"
#include "hex2c_rt.h"

/* PL0BX 800F6F48..800F7080 (312 bytes, slot B base 800F4800). Inputs: $a0 = P (the object), $a1 = a case byte.
 * Writes the state records at P and Q = RD32(P+0x170), then sel = (int8)(a1 - 3). If (unsigned)sel >= 16 it returns 0.
 * Otherwise it does jr through the table at 0x800F4800 (word at 0x800F4800 + 4*sel). The table targets are
 * 800F6FF4 (entries 0, 6, 10), 800F6FFC (12), 800F7018 (15), 800F7038 (2) and 800F7078 (all others). 800F7078 is the
 * epilogue, so $v0 is the table value there (0x800F7078 for those entries).
 * The generated C was a switch on the byte with a default of 0x800F0000 (the lui half of 0x800F4800 without the addiu);
 * the native run returned that value. The lui v0,0x800f / addiu v0,v0,0x4800 pair addresses 0x800F4800.
 * $a2 and $a3 are overwritten before any read, so only $a0 and $a1 are inputs. */
uint32_t man_PL0BX_800F6F48(const GuestMem *m, uint32_t a0, uint32_t a1) {
    uint32_t p = a0, q, t0, p1f6, q1f6, v0, sel, target;

    p1f6 = RD8(p + 0x1F6u);                         /* lbu v1,0x1f6(a3) */
    q = RD32(p + 0x170u);                           /* lw a2,0x170(a3) */
    t0 = RD32(p + 0x130u);                          /* lw t0,0x130(a3) */
    WR8(p + 0x1FAu, 8u);                            /* sb a0(=8),0x1fa(a3) */
    WR8(p + 0x05u, 1u);                             /* sb v0(=1),0x5(a3) */
    WR8(p + 0x06u, 0u);
    WR8(p + 0x08u, 0u);
    WR8(p + 0x07u, 0u);
    WR8(p + 0x206u, a1);                            /* sb a1,0x206(a3) (the original a1) */
    WR8(p + 0x205u, 0u);
    WR8(p + 0x1FBu, 0u);
    WR8(p + 0x0Bu, p1f6);                           /* sb v1,0xb(a3) */

    q1f6 = RD8(q + 0x1F6u);                         /* lbu v1,0x1f6(a2) */
    WR8(q + 0x206u, a1);                            /* sb a1,0x206(a2) (still the original a1) */
    a1 = a1 - 3u;                                   /* addiu a1,a1,-3 */
    WR8(q + 0x1FAu, 8u);
    WR8(q + 0x05u, 1u);
    WR8(q + 0x06u, 0u);
    WR8(q + 0x08u, 0u);
    WR8(q + 0x07u, 0u);
    WR8(q + 0x205u, 0u);
    WR8(q + 0x1FBu, 0u);
    WR8(q + 0x01u, 0u);
    WR8(q + 0x0Bu, q1f6);                           /* sb v1,0xb(a2) */

    v0 = RD8(p + 0x208u);                           /* lbu v0,0x208(a3) */
    WR8(p + 0x207u, v0);                            /* sb v0,0x207(a3) */
    WR8(q + 0x207u, v0);                            /* sb v1(=v0),0x207(a2) */

    sel = (uint32_t)(int32_t)(int8_t)(uint8_t)a1;   /* sll a1,a1,24; sra v1,a1,24 */
    if (sel >= 16u) {                               /* sltiu v0,v1,16; beq v0,zero,800f7078 (v0 = 0 returned) */
        return 0u;
    }
    target = RD32(0x800F4800u + (sel << 2));        /* lui v0,0x800f; addiu v0,v0,18432; lw v0,0(v1); jr v0 */

    if (target == 0x800F6FF4u || target == 0x800F6FFCu) {
        if (target == 0x800F6FF4u) {
            WR8(t0 + 0x20u, 1u);                    /* addiu v0,zero,1; sb v0,0x20(t0) */
        }
        v0 = RD8(p + 0x207u);                       /* 800f6ffc: lbu v0,0x207(a3) */
        WR8(q + 0x207u, v0);                        /* sb v0,0x207(a2) */
        v0 = 1u;                                    /* addiu v0,zero,1 */
        WR8(p + 0x1FBu, 0u);                        /* sb zero,0x1fb(a3) */
        WR8(q + 0x1FBu, 0u);                        /* j 800f7030; delay slot sb zero,0x1fb(a2) */
    } else if (target == 0x800F7018u) {
        WR8(q + 0x207u, 2u);                        /* addiu v0,zero,2; sb v0,0x207(a2) */
        WR8(p + 0x207u, 2u);                        /* sb v0,0x207(a3) */
        v0 = 1u;                                    /* addiu v0,zero,1 */
        WR8(p + 0x1FBu, 1u);                        /* sb v0,0x1fb(a3) */
        WR8(q + 0x1FBu, 1u);                        /* sb v0,0x1fb(a2) (falls into 800f7030) */
    } else if (target == 0x800F7038u) {
        /* jr straight to 800f7038 with $v0 = the target; the copy below does not read $v0 first. */
        v0 = target;
    } else {
        /* 800f7078: the epilogue returns $v0 = the table value. */
        return target;
    }

    /* 800f7030 (common tail) and 800f7038 */
    if (target != 0x800F7038u) {
        WR8(p + 0x205u, v0);                        /* sb v0,0x205(a3) */
        WR8(q + 0x205u, v0);                        /* sb v0,0x205(a2) */
    }
    WR32(q + 0x68u, RD32(p + 0x68u));               /* lwl/lwr t1,0x6b/0x68(a3); swl/swr 0x6b/0x68(a2): aligned word copy */
    WR32(q + 0x6Cu, RD32(p + 0x6Cu));               /* lwl/lwr t1,0x6f/0x6c(a3); swl/swr 0x6f/0x6c(a2) */
    v0 = RD8(p + 0x1F6u);                           /* lbu v0,0x1f6(a3) */
    WR8(q + 0x1F6u, v0);                            /* sb v0,0x1f6(a2) */
    v0 = RD8(p + 0x0Bu);                            /* lbu v0,0xb(a3) */
    WR8(q + 0x0Bu, v0);                             /* sb v0,0xb(a2) */
    return v0;                                      /* 800f7078: jr ra with $v0 = the byte at 0xb */
}
