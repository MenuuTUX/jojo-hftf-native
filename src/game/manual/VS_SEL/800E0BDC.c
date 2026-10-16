#include "manual.h"
#include "hex2c_rt.h"

/* VS_SEL 800E0BDC..800E0C58. $a0 = P. Q = word at P+0x80. Clears byte P+1; if byte Q+2 (signed) is not 1, returns 1 at once.
 * Otherwise stores that byte at P+1 (the delay slot of the bne at 800e0c00 runs on both paths) and then:
 *   byte P+6 == 0: v1 = byte P+0xE8 + 10; if v1 < 255 store v1, else store 1 at P+6 and 255 at P+0xE8; returns 1.
 *   byte P+6 != 0: v1 = byte P+0xE8 - 10; if (int16)v1 < 65 store P+6 = a1-1 and v1 = 64; store v1 at P+0xE8; returns a1-1.
 * The generated C treats $a0 as the wrong object and writes 1 to P+1 and P+0xE8 (its own reading of the code). */
uint32_t man_VS_SEL_800E0BDC(const GuestMem *m, uint32_t a0) {
    uint32_t p = a0;
    uint32_t q = RD32(p + 0x80u);                         /* lw v0,0x80(a0) */
    int32_t a2;
    uint32_t a1, v1;

    WR8(p + 1u, 0u);                                      /* sb zero,1(a0) */
    a2 = RDS8(q + 2u);                                    /* lb a2,2(v0) */
    if (a2 != 1)
        return 1u;                                        /* v0 = 1 (addiu), bne to 800e0c50 */

    v1 = RD8(p + 0xE8u);                                  /* lbu v1,0xe8(a0) */
    a1 = RD8(p + 6u);                                     /* lbu a1,6(a0) */
    WR8(p + 1u, (uint32_t)a2);                            /* sb a2,1(a0): delay slot of bne at 800e0c00, both paths */

    if (a1 != 0u) {
        v1 = v1 - 10u;                                    /* addiu v0,v1,-10; addu v1,v0,zero */
        if ((int32_t)(int16_t)(uint16_t)v1 < 65) {        /* sll/sra 16, slti 65 */
            WR8(p + 6u, a1 - 1u);                         /* sb v0,6(a0); v0 = a1-1 from the beq delay slot */
            v1 = 64u;                                     /* addiu v1,zero,64 */
        }
        WR8(p + 0xE8u, v1);                               /* sb v1,0xe8(a0) at 800e0c4c */
        return a1 - 1u;                                   /* v0 = a1-1 (beq delay slot) */
    }

    v1 = v1 + 10u;                                        /* addiu v0,v1,10; addu v1,v0,zero */
    if ((int32_t)v1 < 255) {                              /* slti v0,v0,255; bne taken to 800e0c4c */
        WR8(p + 0xE8u, v1);
        return 1u;                                        /* v0 = a1+1 = 1 (bne delay slot) */
    }
    WR8(p + 6u, a1 + 1u);                                 /* sb v0,6(a0): v0 = a1+1 */
    v1 = 255u;                                            /* j 800e0c4c, addiu v1,zero,255 */
    WR8(p + 0xE8u, v1);
    return 1u;
}
