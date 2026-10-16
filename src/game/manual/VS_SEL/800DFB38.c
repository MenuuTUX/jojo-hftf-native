#include "manual.h"
#include "hex2c_rt.h"

/* VS_SEL 800DFB38..800DFC48. $a0 = P (kept in s0); $a1 is written (lbu a1,0(P)) before any read, so it is a dummy parameter.
 * $a2/$a3 are never written here and are passed on unchanged to 80024408 (nargs 4, as in 800BBB50). Logic from the MIPS:
 *   v1 = halfword P+0x10. If bit 0x2000: byte P+8 += 1; if signed (byte) < 4, call 80024408(320, byte P+0); else P+8 = 3.
 *   Else if bit 0x8000: byte P+8 -= 1; if signed (byte) >= 0, call 80024408(320, byte P+0); else P+8 = 0.
 *   Both branch delay slots and the 320 call's delay slot run as the MIPS does. Then jal 800e0138; v0 = (result & 0xFFFF) &
 *   (signed byte P+0 + 1); if zero return 0. Otherwise a flag in the byte at 0x800E2F94 selects one of two store sets
 *   (P+2 and P+4), then jal 80024408(321, byte P+0, $a2, $a3) and return its $v0.
 * The call to 800e0138 ignores its arguments (it reads none), so $a0 is passed as P. */
uint32_t man_VS_SEL_800DFB38(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t p = a0;
    uint32_t sp = m->sp_get(m->ctx) - 24u;               /* addiu sp,sp,-24 */
    uint32_t v1, v, t, r0, r;
    int32_t s;
    int call320 = 0;
    (void)a1;
    m->sp_set(m->ctx, sp);

    v1 = RD16(p + 0x10u);                                 /* lhu v1,0x10(s0) */
    if ((v1 & 0x2000u) != 0) {
        v = RD8(p + 8u) + 1u;                             /* lbu v0,8; addiu v0,v0,1; sb v0,8 */
        WR8(p + 8u, v);
        s = (int32_t)(int8_t)(uint8_t)v;                  /* sll 24; sra 24 */
        if (s < 4)
            call320 = 1;                                  /* bne taken: delay slot addiu v0,zero,3 has no store */
        else
            WR8(p + 8u, 3u);                              /* j 800dfbc0 with delay sb v0,8(s0), v0 = 3 */
    } else if ((v1 & 0x8000u) != 0) {
        v = RD8(p + 8u) - 1u;                             /* lbu; addiu v0,v0,-1; sb v0,8 */
        WR8(p + 8u, v);
        s = (int32_t)(int8_t)(uint8_t)v;
        if (s >= 0)
            call320 = 1;                                  /* bgez taken to 800dfbb4 */
        else
            WR8(p + 8u, 0u);                              /* j 800dfbc0, delay sb zero,8(s0) */
    }

    if (call320)
        (void)m->call4(m->ctx, 0x80024408u, 320u, RD8(p), a2, a3);   /* lbu a1,0(s0); jal 80024408; delay addiu a0,zero,320 */

    r0 = m->call4(m->ctx, 0x800E0138u, p, 0u, a2, a3);    /* jal 800e0138, delay slot nop */
    t = (r0 & 0xFFFFu) & (uint32_t)(RDS8(p) + 1);         /* andi v0,v0,0xffff; lb v1,0(s0); addiu v1,v1,1; and */

    if (t == 0u) {
        r = 0u;                                           /* beq v0,zero,800dfc38: return v0 = 0 */
    } else {
        if (RDS8(0x800E2F94u) != 0) {                     /* lb v0,0x2f94(a0) with a0 = 0x800e0000 */
            WR8(p + 2u, RD8(p + 2u) + 1u);                /* lbu v0,2; addiu; sb v0,2 */
            WR8(0x800E2F94u, RD8(p) - 128u);              /* lbu v1,0; addiu v1,-128; sb v1,0x2f94(a0) (j delay slot) */
            WR8(p + 4u, 0x80u);                           /* addiu v0,zero,-128; sb v0,4(s0) */
        } else {
            WR8(p + 2u, 4u);                              /* beq delay addiu v1,zero,4; sb v1,2(s0) */
            WR8(0x800E2F94u, RD8(p) - 128u);              /* lbu v0,0; addiu v0,-128; sb v0,0x2f94(a0) */
            WR8(p + 4u, 1u);                              /* addiu v0,zero,1; sb v0,4(s0) */
        }
        r = m->call4(m->ctx, 0x80024408u, 321u, RD8(p), a2, a3);   /* lbu a1,0(s0); jal 80024408; delay addiu a0,zero,321 */
    }

    m->sp_set(m->ctx, sp + 24u);
    return r;
}
