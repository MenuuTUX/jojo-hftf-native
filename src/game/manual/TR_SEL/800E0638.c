#include "manual.h"
#include "hex2c_rt.h"

/* TR_SEL 800E0638..800E0807 (464 bytes). $a0 = P. No calls. S = 0x80066C48 (lui/addiu at 800e0638), the record of the
 * overlay. $a1 = lw P+0x80 (a pointer; the entry $a1 is never read).
 *
 * Flag (800e0640..800e069c): h = halfword S+4. Bit 0 of byte S+0x22 set: if byte P+4 != 0 the flag is (h-2) < 3 (unsigned),
 * else the flag is raw h. Bit clear: the two cases swap. A nonzero flag stores 1 to P+1.
 * Then byte P+4 < 2 (unsigned) takes the table at 0x800E1FF4 (index = signed byte at a1+0x12), else the table at 0x800E2064
 * (index = signed byte at a1+0xA). Returns the untruncated 32-bit lhu+0x68 or lhu+0x28 stored to P+0x6E. */
uint32_t man_TR_SEL_800E0638(const GuestMem *m, uint32_t a0) {
    uint32_t p = a0;
    const uint32_t s = 0x80066C48u;
    uint32_t bit, a1, flag, h, n, v, t, r;

    bit = RD8(s + 0x22u) & 1u;                           /* lbu v0,0x22(v1); andi v0,v0,1 */
    a1 = RD32(p + 0x80u);                                /* lw a1,0x80(a0) at 800e0644 */

    if (bit) {                                           /* fall-through of beq at 800e0648 */
        if (RD8(p + 4u) != 0u) flag = ((uint32_t)RD16(s + 4u) - 2u) < 3u;    /* 800e068c */
        else                   flag = RD16(s + 4u);                          /* 800e0664, j 800e069c */
    } else {                                             /* beq at 800e0648 taken to 800e0670 */
        if (RD8(p + 4u) == 0u) flag = ((uint32_t)RD16(s + 4u) - 2u) < 3u;    /* 800e068c */
        else                   flag = RD16(s + 4u);                          /* 800e0680, j 800e069c */
    }
    if (flag != 0u) WR8(p + 1u, 1u);                     /* beq at 800e069c; addiu v0,zero,1 is its delay slot */

    if (RD8(p + 4u) < 2u) {                              /* lbu v0,4(a0); sltiu v0,v0,2; beq at 800e06b4 */
        if (RDS8(a1 + 4u) != 0) {                        /* lb v0,4(a1); bne at 800e06c4 */
            WR8(p + 1u, 1u);                             /* 800e0700 sb v0,1(a0), v0 = 1 */
            WR16(p + 0x1Eu, 1u);                         /* 800e0708 sh v0,0x1e(a0) */
        } else {
            n = (uint32_t)RD16(p + 0xCu) - 1u;           /* lhu v0,0xc(a0); addiu v0,v0,-1 */
            WR16(p + 0xCu, n);                           /* sh v0,0xc(a0) */
            if ((n << 16) == 0u) {                       /* sll v0,v0,16; bne v0,zero,800e0710 not taken */
                v = RD16(p + 0x1Eu);                     /* lhu v0,0x1e(a0) */
                WR16(p + 0xCu, 2u);                      /* addiu v1,zero,2; sh v1,0xc(a0) */
                WR16(p + 0x1Eu, (v + 1u) & 3u);          /* addiu v0,v0,1; andi v0,v0,3; sh v0,0x1e(a0) */
            }
        }
        t = 0x800E1FF4u + 4u * (uint32_t)(int32_t)RDS8(a1 + 0x12u);   /* lb v0,0x12(a1); sll 2; addu with v1 = 0x800E1FF4 */
        WR16(p + 0x6Au, RD16(t) + 0xC0u);                /* lhu v0,0(v0); addiu v0,v0,192; sh v0,0x6a(a0) */
        t = 0x800E1FF4u + 4u * (uint32_t)(int32_t)RDS8(a1 + 0x12u);   /* lb v0,0x12(a1) again at 800e0730 */
        r = RD16(t + 2u) + 0x68u;                        /* lhu v0,2(v0); addiu v0,v0,104 */
        WR16(p + 0x6Eu, r);                              /* jr ra; sh v0,0x6e(a0) in its delay slot */
        return r;
    }

    h = RD16(s + 4u);                                    /* lhu v1,0x6c4c(v0) at 800e0754 */
    if (h != 3u) {                                       /* bne v1,v0,800e07bc (v0 = 3); delay addiu v0,zero,1 */
        WR16(p + 0x1Eu, 1u);                             /* 800e07bc sh v0,0x1e(a0) */
    } else {
        n = (uint32_t)RD16(p + 0xCu) - 1u;               /* 800e0764..0770 */
        WR16(p + 0xCu, n);
        if ((n << 16) == 0u) {                           /* sll; bne v0,zero,800e079c not taken */
            v = RD16(p + 0x1Eu);                         /* 800e0780 lhu v0,0x1e(a0) */
            WR16(p + 0xCu, 2u);                          /* addiu v1,zero,2; sh v1,0xc(a0) */
            WR16(p + 0x1Eu, (v + 1u) & 3u);              /* addiu; andi v0,v0,3; sh v0,0x1e(a0) */
        }
        WR8(p + 1u, 1u);                                 /* 800e079c sb v0,1(a0), v0 = 1 */
        WR16(p + 0x10u, (uint32_t)(int32_t)RDS8(a1) + 79u);   /* lbu v0,0(a1); sll/sra 24; addiu 79; sh at 800e07b8 */
    }

    t = 0x800E2064u + 4u * (uint32_t)(int32_t)RDS8(a1 + 0xAu);   /* lb v0,0xa(a1); sll 2; addu with v1 = 0x800E2064 */
    WR16(p + 0x6Au, RD16(t) + 0xC0u);                    /* lhu v0,0(v0); addiu v0,v0,192; sh v0,0x6a(a0) */
    t = 0x800E2064u + 4u * (uint32_t)(int32_t)RDS8(a1 + 0xAu);   /* lb v0,0xa(a1) again at 800e07e4 */
    r = RD16(t + 2u) + 0x28u;                            /* lhu v0,2(v0); addiu v0,v0,40 */
    WR16(p + 0x6Eu, r);                                  /* jr ra; sh v0,0x6e(a0) in its delay slot */
    return r;
}
