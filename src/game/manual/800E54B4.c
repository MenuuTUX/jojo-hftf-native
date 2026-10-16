#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800BC074(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2);

/* 800E54B4 is one function. IDA cut it at 800E56A8 (sub_800E56A8), but the code from 800E56A8 to
 * 800E5914 is the same body: it uses s0 (p), s1 (p+396), s3 (p+0x170) and pops the 40-byte frame at 800E5910.
 * The switch targets 0x5788, 0x5858, 0x587C, 0x58C4 are in that tail. Labels are named after the address.
 * Every path ends in a call result or a value read at the end (see the per-path notes). */
uint32_t man_800E54B4(const GuestMem *m, uint32_t a0) {
    uint32_t p = a0;
    uint32_t s1 = p + 396u;
    uint32_t s3 = RD32(p + 0x170u);
    uint32_t a1v = RD32(p + 0x17Cu);     /* lw a1,0x17c(s0) at 800e54d8: a1 is this value from here on */
    uint32_t state = RD8(p + 7u);
    uint32_t v1, cnt, r;

    if (state >= 8u) {
        return 0u;                       /* sltiu v0,v1,8 is 0: branch to 800e58fc with v0 = 0 */
    }
    switch (state) {
    case 0: goto c0;
    case 1: goto c1;
    case 2: goto c2;
    case 3: goto c3;
    case 4: goto c4;
    case 5: goto c5;
    case 6: goto c6;
    default: goto c7;
    }

c0:
    WR8(p + 7u, state + 1u);
    m->call4(m->ctx, 0x800E5CD0u, p, a1v, 0, 0);
    WR8(p + 0x107u, 60);
    WR16(s1, 0);
    v1 = RD8(p + 0x207u) + 0x800E7E14u;
    WR16(p + 0xEu, 0);
    WR16(p + 0xCu, (uint32_t)(int32_t)(int8_t)RD8(v1));
    return m->call4(m->ctx, 0x800268BCu, p, RD32(0x800EBC14u), 0, 0);

c1:
    if (RDS8(p + 0x4Du) == 0) goto L58F4;
    WR8(p + 0x4Du, 0);
    WR8(p + 7u, state + 1u);             /* sb in the delay slot of jal 80025548: it runs before the callee */
    r = m->call4(m->ctx, 0x80025548u, (RD8(p + 0x136u) + 4u) & 0xFFu, 0, 3, 0);
    v1 = r;
    if (v1 != 0) {
        WR32(v1 + 0x80u, p);
        WR16(v1 + 0xCu, (uint32_t)(int32_t)(int8_t)RD8(p + 0x4Eu));
    }
    m->call4(m->ctx, 0x80024104u, p, 289, 0, 0);
    WR32(p + 0x70u, RD32(0x800E7B80u));
    v1 = RD8(p + 0xBu);
    WR32(p + 0x7Cu, 0);
    WR32(p + 0x74u, 0);
    WR32(p + 0x78u, RD32(0x800E7B84u));
    if (v1 != 0) {
        WR32(p + 0x70u, 0u - RD32(p + 0x70u));
        WR32(p + 0x78u, 0u - RD32(p + 0x78u));
    }
    man_800BC074(m, p, 59, 74);
    m->call4(m->ctx, 0x800BB530u, p, 1, 0, 0);
    goto L58F4;

c2:
    cnt = RD16(s1);
    if (cnt >= 121u) {
        WR8(p + 0x25Cu, 60);
        WR8(p + 0x25Bu, 4);
        WR8(p + 7u, state + 1u);
    }
    goto c3;

c3:
    if ((RD16(p + 0x1D6u) & 0x40u) != 0u) goto L58E4;
    v1 = RD16(s1) + 1u;
    WR16(s1, v1);
    if ((v1 & 0xFFFFu) < 300u) {
        if ((v1 & 7u) == 7u) {
            m->call4(m->ctx, 0x80029714u, p, 13, 6, 0);
        }
    }
    /* 800e56a0 */
    if ((RD16(p + 0x1CCu) & 0xB0u) != 0u) {
        if (RD16(s1) < 151u) goto L58F4;
    }
    /* 800e56c8 */
    WR8(p + 7u, 4);
    WR8(s3 + 0x21Fu, 1);
    m->call4(m->ctx, 0x80029714u, p, 0, 1, 0);
    m->call4(m->ctx, 0x80024104u, p, 278, 0, 0);
    if (RD16(s1) < 30u) WR8(p + 0x207u, 0);
    if (RD16(s1) < 60u) WR8(p + 0x207u, 1);
    if (RD16(s1) < 120u) {
        WR8(p + 0x207u, 2);
    } else {
        WR8(p + 0x207u, 3);
        WR16(p + 0xEu, 1);
    }
    /* 800e574c */
    v1 = RD8(p + 0x207u) + 0x800E7E10u;
    m->call4(m->ctx, 0x800BB2A4u, p, RD8(v1), 0, 0);
    m->call4(m->ctx, 0x800BB30Cu, p, 0, 0, 0);
    m->call4(m->ctx, 0x800268BCu, p, RD32(0x800EBC18u), 0, 0);
    goto L58AC;

c4:
    WR16(p + 0xCu, RD16(p + 0xCu) - 1u);
    if (RDS8(p + 0xCu) >= 0) {
        r = m->call4(m->ctx, 0x800E5CF4u, p, a1v, 80, 0);
        if (r == 0) goto L5848;
    }
    /* 800e57b8 */
    WR8(p + 7u, state + 1u);
    WR32(p + 0x70u, RD32(0x800E7BA4u));
    v1 = RD8(p + 0xBu);
    WR32(p + 0x78u, RD32(0x800E7BA8u));
    if (v1 != 0) {
        WR32(p + 0x70u, 0u - RD32(p + 0x70u));
        WR32(p + 0x78u, 0u - RD32(p + 0x78u));
    }
    r = m->call4(m->ctx, 0x80025548u, (RD8(p + 0x136u) + 4u) & 0xFFu, 0, 2, 0);
    v1 = r;
    if (v1 != 0) {
        WR32(v1 + 0x80u, p);
    }
    m->call4(m->ctx, 0x80024104u, p, 290, 0, 0);
    m->call4(m->ctx, 0x800268BCu, p, RD32(0x800EBC1Cu), 0, 0);
    /* falls into 800e5848 */

L5848:
    m->call4(m->ctx, 0x800BB648u, p, 0, 0, 0);
    goto L58A4;

c5:
    if (RDS8(p + 0x4Du) != -1) goto L5848;
    WR8(p + 0x4Du, 0);
    WR8(p + 7u, state + 1u);
    goto L5848;

c6:
    if (RDS8(p + 0x4Du) == -1) {
        WR8(p + 7u, state + 1u);
        WR8(p + 0x4Du, 0);
        WR8(s3 + 0x21Fu, 1);
    }
    goto L58A4;

c7:
    r = m->call4(m->ctx, 0x800B46A4u, p, 0, 0, 0);
    if (r != 0) return 0xFFFFFFFFu;      /* the addiu v0,zero,-1 delay slot of bne 800e58cc */
    if (RDS8(p + 0x4Du) != -1) goto L58F4;
    /* 800e58e4 */
L58E4:
    return m->call4(m->ctx, 0x800C1F38u, p, 0, 0, 0);

L58A4:
    m->call4(m->ctx, 0x80026A04u, p, 0, 0, 0);
    /* falls into 800e58ac */

L58AC:
    v1 = (uint32_t)(int32_t)(int16_t)RD16(p + 0xEu);
    if (v1 == 0u) return 0u;
    WR8(p + 0x13Cu, 0);
    return v1;

L58F4:
    return m->call4(m->ctx, 0x80026A04u, p, 0, 0, 0);
}
