#include "manual.h"
#include "hex2c_rt.h"

uint32_t man_800B4FCC(const GuestMem *m, uint32_t a0, uint32_t in_v0);

uint32_t man_800C5004(const GuestMem *m, uint32_t a0) {
    uint32_t p = a0;
    uint32_t state = RD8(p + 7);
    uint32_t v0, r, t7, t1fc;

    if (state == 0) {
        WR8(p + 7, state + 1);              /* sb v0,0x7(s0) at 800c502c */
        WR16(p + 0xC, 16);                  /* sh v0,0xc(s0): delay slot of jal 800b4fcc */
        man_800B4FCC(m, p, 16u);            /* in_v0 = 16: $v0 is 16 at the jal */
        m->call4(m->ctx, 0x800C6FF8u, p, 0, 0, 0);
        m->call4(m->ctx, 0x80024104u, p, 107, 0, 0);
        m->call4(m->ctx, 0x80024104u, p, 263, 0, 0);
        return m->call4(m->ctx, 0x800B4B58u, p, 49, 52, 0);
    }
    if (state == 1) {
        m->call4(m->ctx, 0x80026A04u, p, 0, 0, 0);
        v0 = RD16(p + 0xC) - 1u;            /* lhu v0,0xc(s0); addiu v0,v0,-1 */
        WR16(p + 0xC, v0);                  /* sh v0,0xc(s0) */
        v0 = v0 << 16;                      /* sll v0,v0,16 */
        if (v0 != 0) {
            return v0;                      /* bne at 800c509c: returns the shifted value */
        }
        t7 = RD8(p + 7);
        t1fc = RD8(p + 0x1FC);
        WR8(p + 7, t7 + 1);                 /* sb v0,0x7(s0): delay slot of beq at 800c50b0 */
        if (t1fc == state) {
            return m->call4(m->ctx, 0x800B4AD0u, p, 54, 0, 0);
        }
        return m->call4(m->ctx, 0x800B4AD0u, p, 51, 0, 0);
    }
    r = m->call4(m->ctx, 0x800B46A4u, p, 0, 0, 0);
    if (r != 0) {
        return 0xFFFFFFFFu;                 /* addiu v0,zero,-1 is the delay slot of bne at 800c50e8 */
    }
    if (RDS8(p + 0x4D) == -1) {
        return m->call4(m->ctx, 0x800C1F38u, p, 0, 0, 0);
    }
    return m->call4(m->ctx, 0x80026A04u, p, 0, 0, 0);
}
