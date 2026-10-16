#include "manual.h"
#include "hex2c_rt.h"

/* 8002A8A8: per-frame step of the 0x80066D54 sequencer (byte counter), two 16-bit accumulators 0x80066D4E and 0x80066D52,
 * table walks at 0x800549FC (mode != 2) and 0x80054BDC (mode 2), mode word 0x8005AFE8. Returns $v0 as the original leaves it:
 * 0x80060000 on the reset path; for mode 2 the old counter + 1 (the value stored back is the same); otherwise 3, or the
 * byte 0x80066D55 xor 1 when the mode is 3. */
uint32_t man_8002A8A8(const GuestMem *m) {
    uint32_t result, old, newb, idx, tbl;
    uint8_t  b55;
    int32_t  w58, w5a;
    (void)m;

    if (RD8(0x80066D54u) == 16) {
        WR16(0x80066D5Au, 0);
        WR16(0x80066D58u, 0);
        WR8(0x80066D55u, 0);
        WR8(0x80066D54u, 0);
        WR16(0x8005AFE8u, 0);
        return 0x80060000u;                       /* lui v0,0x8006 leaves $v0 = 0x80060000 */
    }

    if (RD16(0x8005AFE8u) == 2u) {
        old = RD8(0x80066D54u);
        w5a = (int32_t)(int16_t)RD16(0x80066D5Au);
        idx = 0x80054BDCu + (uint32_t)(w5a * 32) + (uint32_t)((int32_t)(int8_t)old * 2);
        tbl = RD16(idx);
        WR8(0x80066D54u, old + 1u);
        WR16(0x80066D52u, RD16(0x80066D52u) + tbl);
        return old + 1u;                          /* $v0 = old counter + 1 (the stored value is the low byte of this) */
    }

    old = RD8(0x80066D54u);
    w58 = (int32_t)(int16_t)RD16(0x80066D58u);
    idx = 0x800549FCu + (uint32_t)(w58 * 32) + (uint32_t)((int32_t)(int8_t)old * 2);
    tbl = RD16(idx);
    newb = old + 1u;                              /* a2 after addiu; only its low byte is stored */
    WR8(0x80066D54u, newb);
    WR16(0x80066D4Eu, RD16(0x80066D4Eu) + tbl);
    result = 3u;

    if (RD16(0x8005AFE8u) == 3u) {
        b55 = RD8(0x80066D55u);
        result = (uint32_t)(b55 ^ 1u);
        if (b55 == 0) {
            w5a = (int32_t)(int16_t)RD16(0x80066D5Au);
            idx = 0x80054BDCu + (uint32_t)(w5a * 32) + (uint32_t)((int32_t)(int8_t)(uint8_t)newb * 2);
            WR16(0x80066D52u, RD16(0x80066D52u) + RD16(idx));
        }
        WR8(0x80066D55u, result);
    }
    return result;
}
