#include "manual.h"
#include "hex2c_rt.h"

/* 800A789C (AC_MAIN). Reads no argument register (nargs 0). Calls pass $a0 only where the code sets it; the other
 * argument registers are passed as 0. The callees without arguments in the pseudocode ignore them. */
uint32_t man_800A789C(const GuestMem *m) {
    uint32_t v;
    uint32_t top;
    uint32_t b;
    int skip5;

    m->call4(m->ctx, 0x80024C7Cu, 0u, 0u, 0u, 0u);
    m->call4(m->ctx, 0x800A7B50u, 0u, 0u, 0u, 0u);
    if (RDS8(0x80066C6Cu) == 3)
        m->call4(m->ctx, 0x800A2138u, 0x800CDD50u + 912u * (uint32_t)RDS8(0x8006D2EBu), 0u, 0u, 0u);

    if (RD8(0x80066CB1u) == 0) {
        if (RDS8(0x80066CC6u) != 0 || RD8(0x80066CC9u) != 0) {
            if ((RD8(0x80066C60u) & 1u) != 0)
                m->call4(m->ctx, 0x800AE550u, 0u, 0u, 0u, 0u);
        } else if ((RD8(0x80066CC3u) & 0x80u) == 0) {
            m->call4(m->ctx, 0x800AE550u, 0u, 0u, 0u, 0u);
        }

        m->call4(m->ctx, 0x800A7DB8u, 0u, 0u, 0u, 0u);
        m->call4(m->ctx, 0x800293ACu, 0u, 0u, 0u, 0u);

        top = RD8(0x80066CC3u);
        if (top & 0x80u) {
            m->call4(m->ctx, 0x80029300u, (top & 0x7Fu) ? 5u : 4u, 0u, 0u, 0u);
        } else if (!((RDS8(0x80066CC6u) != 0 || RD8(0x80066CC9u) != 0) && (RD8(0x80066C60u) & 1u) == 0)) {
            b = RD8(0x80066CB0u);
            if (b == 0u || (b & 1u) != 0u)
                m->call4(m->ctx, 0x80029300u, 2u, 0u, 0u, 0u);
            b = RD8(0x80066CB0u);
            if (b == 0u || (b & 2u) != 0u)
                m->call4(m->ctx, 0x80029300u, 3u, 0u, 0u, 0u);
            m->call4(m->ctx, 0x80029300u, 4u, 0u, 0u, 0u);

            skip5 = 0;
            if (RDS8(0x800CE217u) == 10) {
                b = RD8(0x80066CB0u);
                skip5 = (b != 0u && (b & 2u) == 0u);
            }
            if (!skip5)
                m->call4(m->ctx, 0x80029300u, 5u, 0u, 0u, 0u);
        }

        /* LABEL_34 */
        m->call4(m->ctx, 0x80029300u, 6u, 0u, 0u, 0u);
        m->call4(m->ctx, 0x80029300u, 7u, 0u, 0u, 0u);
        m->call4(m->ctx, 0x800A7630u, 0u, 0u, 0u, 0u);
    } else {
        if ((RD8(0x80066CC9u) == 0 || (RD8(0x80066C60u) & 1u) != 0) && (RD8(0x80066CC3u) & 0x80u) == 0)
            m->call4(m->ctx, 0x800AE550u, 0u, 0u, 0u, 0u);
        v = (uint32_t)(uint8_t)(RD8(0x80066CB1u) - 1u);
        WR8(0x80066CB1u, v);
        if ((int32_t)(v << 24) <= 0)
            WR8(0x80066CB0u, 0u);
        else
            m->call4(m->ctx, 0x80029300u, 6u, 0u, 0u, 0u);
    }

    /* LABEL_35 */
    if (RDS8(0x80066C6Cu) == 3) {
        m->call4(m->ctx, 0x800A1D68u, 0u, 0u, 0u, 0u);
        m->call4(m->ctx, 0x800A54F0u, 0u, 0u, 0u, 0u);
        return m->call4(m->ctx, 0x800A1F34u, 0x800CDD50u + 912u * (uint32_t)RDS8(0x8006D2EBu), 0u, 0u, 0u);
    }
    return 3u;
}
