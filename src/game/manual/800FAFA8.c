#include "manual.h"
#include "hex2c_rt.h"

/* 800FAFA8: state machine. Two jump tables in the game data:
   T1 at 0x800F48D8 (taken when the byte at +0x206 == 10): case 0 -> B0, 1 -> B1, 2 -> B2, 3 -> B3, 4 -> B4.
   T2 at 0x800F48F0 (otherwise):                           case 0 -> C0, 1 -> C1, 2 -> C2, 3 -> C3, 4 -> C4.
   Every path that the MIPS reaches at 0x800FB528 is "tail": it calls 0x80026A04 and returns its $v0. */
uint32_t man_800FAFA8(const GuestMem *m, uint32_t a0) {
    uint32_t s0 = a0;
    uint32_t s3 = RD32(s0 + 0x170u);
    uint32_t s1 = RD32(s0 + 0x17Cu);
    uint32_t s2, t, idx, v1;

    if (RD8(s0 + 7u) != 0) goto blockB;

    /* block A: a0+7 == 0 */
    if (RD8(s0 + 8u) == 0) {
        m->call4(m->ctx, 0x800268BCu, s0, RD32(0x801020E8u), 0, 0);
        WR16(s0 + 0x6Eu, 51);
        v1 = RD16(s0 + 0x84u);
        WR8(s0 + 0x25Cu, 0);
        WR8(s0 + 8u, RD8(s0 + 8u) + 1);
        WR16(s0 + 0x1Eu, v1);
    }

    s2 = RD8(s0 + 8u);
    if (s2 == 1) {
        m->call4(m->ctx, 0x80026A04u, s0, 0, 0, 0);
        if (RDS8(s0 + 0x4Du) == 2) {
            WR8(s0 + 0x4Du, 0);
            m->call4(m->ctx, 0x80024104u, s3, 298, 0, 0);
            m->call4(m->ctx, 0x800FB7E8u, s0, 9, 0, 0);
        }
        if (RDS8(s0 + 0x4Du) == 1) {
            m->call4(m->ctx, 0x800BC384u, s0, 16, 0, 0);
            m->call4(m->ctx, 0x80024104u, s0, 16, 0, 0);
            v1 = RD8(s0 + 0xBu);
            WR8(s1 + 7u, 22);
            WR8(s1 + 6u, 1);
            WR8(s1 + 8u, 0);
            WR8(s1 + 0x149u, 6);
            WR8(s1 + 0x14Au, 6);
            WR8(s1 + 0x163u, 0);
            WR8(s1 + 0x14Bu, 1);
            WR8(s1 + 0x106u, v1);
            WR32(s0 + 0x74u, 0xFFFE0000u);
            WR8(s0 + 0x162u, 0);
            WR8(s0 + 8u, RD8(s0 + 8u) + 1);
            WR8(s3 + 0x162u, 0);
        }
        goto L15c;
    }

    if (s2 == 2) {
        if (RDS16(s0 + 0x6Eu) < 36) {
            WR16(s0 + 0x6Eu, 35);
        } else {
            WR32(s0 + 0x6Cu, RD32(s0 + 0x6Cu) + RD32(s0 + 0x74u));
        }
        m->call4(m->ctx, 0x80026A04u, s0, 0, 0, 0);
        if (RDS8(s0 + 0x4Du) != -1) return 0xFFFFFFFFu;
        if (RD8(s0 + 0x1FBu) != 0) {
            m->call4(m->ctx, 0x800C1F38u, s0, 0, 0, 0);
            WR16(s0 + 0x6Eu, 35);
            return 35;
        }
        WR8(s0 + 8u, RD8(s0 + 8u) + 1);
        m->call4(m->ctx, 0x80024104u, s0, 109, 0, 0);
        m->call4(m->ctx, 0x800AF7ECu, s3, 0, 0, 0);
        goto L15c;
    }

    /* s2 other than 1 and 2 falls through to L15c */
L15c:
    if (RD8(s0 + 8u) != 3) return 3;
    m->call4(m->ctx, 0x80026A04u, s0, 0, 0, 0);
    return m->call4(m->ctx, 0x800FB6ECu, s0, 0, 0, 0);

    /* block B: a0+7 != 0 and T1 (byte +0x206 == 10) */
blockB:
    if (RD8(s0 + 0x206u) != 10) goto blockC;
    idx = RD8(s0 + 8u);
    if (idx >= 5) goto tail;
    switch (idx) {
        case 0: goto B0;
        case 1: goto B1;
        case 2: goto B2;
        case 3: goto B3;
        default: goto B4;
    }

B0:
    m->call4(m->ctx, 0x80024104u, s3, 108, 0, 0);
    m->call5(m->ctx, 0x800C80F8u, s0, 3, 83, 71, 12);
    m->call4(m->ctx, 0x800FB7E8u, s0, 11, 0, 0);
    m->call4(m->ctx, 0x800268BCu, s0, RD32(0x801023A8u), 0, 0);
    WR8(s0 + 8u, RD8(s0 + 8u) + 1);
B1:
    if (RDS8(s0 + 0x4Du) == 0) goto tail;
    WR8(s0 + 8u, RD8(s0 + 8u) + 1);
    m->call4(m->ctx, 0x80024104u, s0, 270, 0, 0);
    WR16(s0 + 0xCu, 30);
B2:
    t = (uint32_t)(int32_t)(int16_t)(RD16(s0 + 0xCu) - 1);
    WR16(s0 + 0xCu, t);
    if ((int32_t)t != -1) goto tail;
    m->call4(m->ctx, 0x80024104u, s0, 297, 0, 0);
    m->call4(m->ctx, 0x800BC384u, s0, 16, 0, 0);
    m->call4(m->ctx, 0x800268BCu, s0, RD32(0x801023ACu), 0, 0);
    m->call4(m->ctx, 0x800FB7E8u, s0, 10, 0, 0);
    m->call4(m->ctx, 0x800FB83Cu, s0, 0, 0, 0);
    WR16(s0 + 0xCu, 30);
    WR8(s0 + 8u, RD8(s0 + 8u) + 1);
    v1 = RD8(s0 + 0xBu);
    WR8(s1 + 0x106u, v1);
    WR8(s1 + 7u, 22);
    WR8(s1 + 6u, 1);
    WR8(s1 + 8u, 0);
    WR8(s1 + 0x149u, 0);
    WR8(s1 + 0x14Au, 6);
    WR8(s1 + 0x163u, 0);
    WR8(s1 + 0x14Bu, 1);
    WR8(s1 + 0x159u, 1);
    WR8(s0 + 0x162u, 0);
    WR8(s3 + 0x162u, 0);
    WR8(s0 + 0x204u, 1);
    m->call4(m->ctx, 0x8002B450u, 5, 2, 0, 0);
B3:
    t = (uint32_t)(int32_t)(int16_t)(RD16(s0 + 0xCu) - 1);
    WR16(s0 + 0xCu, t);
    if ((int32_t)t != -1) goto tail;
    WR8(s0 + 8u, RD8(s0 + 8u) + 1);
    WR8(s3 + 0x204u, 1);
    WR8(s3 + 0x255u, 0);
B4:
    if (RDS8(s0 + 0x4Du) != -1) goto tail;
    m->call4(m->ctx, 0x800C1F38u, s0, 0, 0, 0);
    goto tail;

    /* block C: a0+7 != 0 and T2 */
blockC:
    idx = RD8(s0 + 8u);
    if (idx >= 5) goto tail;
    switch (idx) {
        case 0: goto C0;
        case 1: goto C1;
        case 2: goto C2;
        case 3: goto C3;
        default: goto C4;
    }

C0:
    m->call4(m->ctx, 0x80024104u, s3, 108, 0, 0);
    m->call5(m->ctx, 0x800C80F8u, s0, 3, 83, 71, 12);
    m->call4(m->ctx, 0x800FB7E8u, s0, 11, 0, 0);
    m->call4(m->ctx, 0x800268BCu, s0, RD32(0x801023A8u), 0, 0);
    WR8(s0 + 8u, RD8(s0 + 8u) + 1);
C1:
    if (RDS8(s0 + 0x4Du) == 0) goto tail;
    WR8(s0 + 8u, RD8(s0 + 8u) + 1);
    m->call4(m->ctx, 0x80024104u, s0, 270, 0, 0);
    WR16(s0 + 0xCu, 30);
C2:
    t = (uint32_t)(int32_t)(int16_t)(RD16(s0 + 0xCu) - 1);
    WR16(s0 + 0xCu, t);
    if ((int32_t)t == -1) goto C2m1;
    if ((int32_t)t == 8) {
        WR8(s0 + 0x204u, 0);
        WR8(s3 + 0x204u, 0);
    }
    goto tail;
C2m1:
    m->call4(m->ctx, 0x80024104u, s3, 297, 0, 0);
    m->call4(m->ctx, 0x800BC384u, s0, 16, 0, 0);
    m->call4(m->ctx, 0x800268BCu, s0, RD32(0x801023ACu), 0, 0);
    m->call4(m->ctx, 0x800FB7E8u, s0, 10, 0, 0);
    m->call4(m->ctx, 0x800FB83Cu, s0, 0, 0, 0);
    WR16(s0 + 0xCu, 30);
    WR8(s0 + 8u, RD8(s0 + 8u) + 1);
    v1 = RD8(s0 + 0xBu);
    WR8(s1 + 0x106u, v1);
    WR8(s1 + 7u, 22);
    WR8(s1 + 6u, 1);
    WR8(s1 + 8u, 0);
    WR8(s1 + 0x149u, 0);
    WR8(s1 + 0x14Au, 6);
    WR8(s1 + 0x159u, 1);
    WR8(s1 + 0x163u, 0);
    WR8(s1 + 0x14Bu, 1);
    WR8(s0 + 0x162u, 0);
    WR8(s3 + 0x162u, 0);
    WR8(s3 + 0x204u, 1);
    m->call4(m->ctx, 0x8002B450u, 5, 2, 0, 0);
C3:
    t = (uint32_t)(int32_t)(int16_t)(RD16(s0 + 0xCu) - 1);
    WR16(s0 + 0xCu, t);
    if ((int32_t)t != -1) goto tail;
    WR8(s0 + 8u, RD8(s0 + 8u) + 1);
    m->call4(m->ctx, 0x80024104u, s0, 109, 0, 0);
C4:
    m->call4(m->ctx, 0x800FB6ECu, s0, 0, 0, 0);
    goto tail;

tail:
    return m->call4(m->ctx, 0x80026A04u, s0, 0, 0, 0);
}
