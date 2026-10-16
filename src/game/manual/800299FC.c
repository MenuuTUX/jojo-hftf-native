#include "manual.h"
#include "hex2c_rt.h"

/* 800299FC (TEXT, 1376 bytes). Frame 0x68. $a0 = record pointer. The handler at off_8005AFFC[byte +3 of the record]
 * is called with $a0 (the record) and $a1..$a3 as received. Then 8002A9B8 is called, and the switch on byte 0x8006D2D1
 * (cases 9..27) goes through the table at 0x80054E48. The table holds four targets: 80029F44 (epilogue, $v0 stays the
 * table entry 0x80029F44), 80029E48, 80029E84, 80029AD0. Any other byte returns through the delay-slot lui 0x80050000.
 * The epilogue returns whatever $v0 holds at that point; each path below sets it the way the MIPS does. */

static void copy_bytes(const GuestMem *m, uint32_t dst, uint32_t src, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++)
        WR8(dst + i, RD8(src + i));
}

uint32_t man_800299FC(const GuestMem *m, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t sp = m->sp_get(m->ctx) - 0x68u;   /* the original "addiu sp,sp,-104" */
    uint32_t v0, v1, p, p2, t, c0, x;

    m->sp_set(m->ctx, sp);

    /* 80029A00..80029A40: if word_8005AFE2 == 0 and (word_8005AFE4 + 1) & 0xFFFF >= 2, word_8005AFE4-- */
    v0 = RD16(0x8005AFE2u);
    if (v0 == 0u) {
        v1 = RD16(0x8005AFE4u);
        if (((v1 + 1u) & 0xFFFFu) >= 2u)
            WR16(0x8005AFE4u, (v1 - 1u) & 0xFFFFu);
    }

    /* 80029A44..80029A64: off_8005AFFC[*(a0 + 3)](a0), with a1..a3 unchanged */
    v0 = RD32(0x8005AFFCu + ((uint32_t)RD8(a0 + 3u) << 2));
    m->calli(m->ctx, v0, a0, a1, a2, a3);

    /* 80029A68..80029A94: sub_8002A9B8(a0, 0x8008EBD8 + 6912 * dword_80066E14) */
    v0 = RD32(0x80066E14u);
    a1 = 0x8008EBD8u + 6912u * v0;
    m->call4(m->ctx, 0x8002A9B8u, a0, a1, a2, a3);

    /* 80029A98..80029AB0: switch on byte_8006D2D1 - 9 (unsigned, range 0..18) */
    v1 = (uint32_t)RD8(0x8006D2D1u) - 9u;
    if (v1 >= 19u) {
        v0 = 0x80050000u;   /* 80029AAC beq taken, delay slot lui v0,0x8005 */
        goto done;
    }
    v0 = RD32(0x80054E48u + (v1 << 2));
    switch (v0) {
    case 0x80029F44u:
        goto done;          /* jr v0 into the epilogue: $v0 = 0x80029F44 */
    case 0x80029E48u:
        goto L_E48;
    case 0x80029E84u:
        goto L_E84;
    case 0x80029AD0u:
        goto L_AD0;
    default:
        goto done;          /* not reached: the table holds only the four targets above */
    }

L_E48:
    /* 80029E48..80029E7C: sub_8002A9B8(0x80066D70, 0x800921D8 + 6912 * dword_80066E14); then epilogue */
    v0 = RD32(0x80066E14u);
    v0 = m->call4(m->ctx, 0x8002A9B8u, 0x80066D70u, 0x800921D8u + 6912u * v0, a2, a3);
    goto done;

L_E84:
    /* 80029E84..80029EB4: same call as E48 */
    v0 = RD32(0x80066E14u);
    m->call4(m->ctx, 0x8002A9B8u, 0x80066D70u, 0x800921D8u + 6912u * v0, a2, a3);
    /* 80029EB8..80029EC8 */
    if ((int8_t)RD8(0x80066C67u) != 0) {
        v0 = 0x80060000u;   /* delay-slot lui v0,0x8006 runs before the branch */
        goto done;
    }
    /* 80029ECC..80029ED4 */
    v0 = RD8(0x80066CA2u);
    if (v0 != 0u)
        goto done;
    /* 80029EDC..80029EF4: counter byte_8008EBCA-- ; return (counter-1) << 24 if its sign bit is clear */
    v0 = (uint32_t)RD8(0x8008EBCAu) - 1u;
    WR8(0x8008EBCAu, v0);
    v0 = v0 << 24;
    if ((int32_t)v0 >= 0)
        goto done;
    /* 80029EF8..80029F40: sh 5 at sp+0x20, sh 28 at sp+0x22, then one 800276F4 call */
    WR16(sp + 0x20u, 5u);
    WR16(sp + 0x22u, 28u);
    v0 = RD8(0x8008EBCBu);
    WR8(0x8008EBCAu, 6u);
    v0 = v0 ^ 1u;
    WR8(0x8008EBCBu, v0);
    x = RD16(sp + 0x20u + (uint32_t)((int32_t)(int8_t)(v0 & 0xFFu) * 2));
    v0 = m->call5(m->ctx, 0x800276F4u, 0u, 5u, x, 16u, 1u);
    goto done;

L_AD0:
    /* 80029AD0..80029AF0 */
    if ((int8_t)RD8(0x80066C67u) != 0) {
        v0 = 0x80060000u;   /* delay-slot lui v0,0x8006 */
        goto done;
    }
    v0 = RD8(0x80066CA2u);
    if (v0 != 0u)
        goto done;
    /* 80029AF0..80029B0C: counter byte_8008EBCA-- ; a1 = 0x80090000 (delay slot) */
    v0 = (uint32_t)RD8(0x8008EBCAu) - 1u;
    WR8(0x8008EBCAu, v0);
    t = v0 << 24;
    a1 = 0x80090000u;
    if ((int32_t)t < 0) {
        /* block A, 80029B10..80029B9C */
        copy_bytes(m, sp + 0x20u, 0x80054DF8u, 8u);
        v1 = 2u;
        v0 = RD8(0x8008EBCBu);
        WR8(0x8008EBCAu, v1);
        v0 = v0 ^ 1u;
        WR8(0x8008EBCBu, v0);
        p = sp + 0x20u;
        p2 = (uint32_t)((int32_t)(int8_t)(v0 & 0xFFu) * 2);
        x = RD16(p + p2);
        m->call5(m->ctx, 0x800276F4u, 0u, 21u, x, 16u, 1u);
        /* 80029B7C..80029B9C */
        v0 = (uint32_t)(int32_t)(int8_t)RD8(0x8008EBCBu);
        p2 = p + (v0 << 1);
        x = RD16(p2 + 4u);
        m->call5(m->ctx, 0x800276F4u, 0u, 93u, x, 16u, 1u);
        a1 = 0x80090000u;   /* 80029BA0 lui a1,0x8009 */
    }

    /* 80029BA4..80029BBC (reached by fall-through or the bgez at 80029B08): counter byte_8008EBCC-- ; 0x80050000 in $v0 (delay slot) */
    v0 = (uint32_t)RD8(0x8008EBCCu) - 1u;
    WR8(0x8008EBCCu, v0);
    t = v0 << 24;
    v0 = 0x80050000u;
    if ((int32_t)t >= 0)
        goto L_D58;
    /* block B, 80029BC0..80029D54 */
    copy_bytes(m, sp + 0x28u, 0x80054E00u, 16u);
    copy_bytes(m, sp + 0x38u, 0x80054E10u, 16u);
    v1 = (uint32_t)(int32_t)(int8_t)RD8(0x8008EBCBu);
    copy_bytes(m, sp + 0x48u, 0x80054E20u, 16u);
    c0 = (v1 != 0u) ? 16u : 22u;
    /* 80029CB4..80029CE0: byte_8008EBCD cycles 0..7 */
    v0 = (uint32_t)(int32_t)(int8_t)RD8(0x8008EBCDu);
    v1 = RD8(0x8008EBCDu);
    v0 = v0 + 1u;
    WR8(0x8008EBCCu, c0);
    if ((int32_t)v0 < 8)
        v0 = v1 + 1u;
    else
        v0 = 0u;
    WR8(0x8008EBCDu, v0);
    /* 80029CF0..80029D04 */
    v0 = (uint32_t)(int32_t)(int8_t)RD8(0x8008EBCDu);
    x = RD16(sp + 0x28u + (v0 << 1));
    m->call5(m->ctx, 0x800276F4u, 0u, 24u, x, 80u, 1u);
    /* 80029D10..80029D2C */
    v0 = (uint32_t)(int32_t)(int8_t)RD8(0x8008EBCDu);
    x = RD16(sp + 0x38u + (v0 << 1));
    m->call5(m->ctx, 0x800276F4u, 0u, 74u, x, 48u, 1u);
    /* 80029D34..80029D54 */
    v0 = (uint32_t)(int32_t)(int8_t)RD8(0x8008EBCDu);
    x = RD16(sp + 0x48u + (v0 << 1));
    m->call5(m->ctx, 0x800276F4u, 0u, 86u, x, 48u, 1u);

L_D58:
    /* 80029D58..80029D74: counter byte_8008EBCE-- ; $v0 = 0x80050000 (delay slot) */
    v0 = (uint32_t)RD8(0x8008EBCEu) - 1u;
    WR8(0x8008EBCEu, v0);
    t = v0 << 24;
    v0 = 0x80050000u;
    if ((int32_t)t >= 0)
        goto done;
    /* block C, 80029D78..80029E40 */
    copy_bytes(m, sp + 0x20u, 0x80054E30u, 8u);
    v1 = (uint32_t)(int32_t)(int8_t)RD8(0x8008EBCFu);
    copy_bytes(m, sp + 0x28u, 0x80054E38u, 16u);
    v0 = sp + v1;
    v1 = v1 + 1u;
    v0 = RD8(v0 + 0x20u);
    v1 = ((int32_t)v1 < 8) ? 1u : 0u;
    WR8(0x8008EBCEu, v0);
    v0 = RD8(0x8008EBCFu);
    v0 = (v1 != 0u) ? v0 + 1u : 0u;
    WR8(0x8008EBCFu, v0);
    /* 80029E28..80029E40 */
    v0 = (uint32_t)(int32_t)(int8_t)RD8(0x8008EBCFu);
    x = RD16(sp + 0x28u + (v0 << 1));
    v0 = m->call5(m->ctx, 0x800276F4u, 0u, 32u, x, 48u, 1u);
    goto done;

done:
    m->sp_set(m->ctx, sp + 0x68u);   /* before every return */
    return v0;
}
