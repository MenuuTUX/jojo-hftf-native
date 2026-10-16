/* Native start-up (Phase 2 step 7, development hybrid). `start` (0x8001000C) is the only caller of main and sits in main's own shard,
 * so its call to main never reaches main's wrapper. This replacement does exactly what `start` does and then enters main through the
 * dispatcher, so the native main (game/manual/800100C8.c) can run. Switch: JOJO_GAME containing "text" and main allowed natively
 * (banked or named in JOJO_LIVE_EXTRA); otherwise the original body.
 *   clear 0x800636D8..0x800991B8; sp = word[0x800100BC] | 0x80000000; word[0x800636D8] = ra; gp = 0x800636D8; fp = sp;
 *   InitHeap(0x800991B8 + 4, word[0x800100BC] - word[0x80062FA0] - 0x000991B8); ra = word[0x800636D8]; main() (does not return). */
#include "jojo_game_glue.h"

extern void jojo_lle_func_8001000C(CPUState *cpu);

void func_8001000C(CPUState *cpu) {
    uint32_t a, top;
    if (!jojo_game_on("text") || !jojo_live_extra(0x800100C8u)) { jojo_lle_func_8001000C(cpu); return; }
    for (a = 0x800636D8u; a < 0x800991B8u; a += 4u) cpu->write_word(a, 0);
    {   /* the original clear loop's guest time (sw, addiu, sltu, bne, nop per word), so main starts on the same frame */
        const char *e = getenv("JOJO_START_CYCLES_PER_WORD");
        jojo_cycles_charge((uint32_t)(((0x800991B8u - 0x800636D8u) / 4u) * (e ? (uint32_t)atoi(e) : 5u)));
    }
    top = cpu->read_word(0x800100BCu);
    cpu->gpr[29] = top | 0x80000000u;
    cpu->write_word(0x800636D8u, cpu->gpr[31]);
    cpu->gpr[28] = 0x800636D8u;
    cpu->gpr[30] = cpu->gpr[29];
    psx_mod_call_guest(cpu, 0x80035A30u, JOJO_GAME_CALL_RET, 0x800991B8u + 4u,          /* InitHeap */
                       top - cpu->read_word(0x80062FA0u) - 0x000991B8u, 0, 0);
    cpu->gpr[31] = cpu->read_word(0x800636D8u);
    psx_dispatch_call(cpu, 0x800100C8u, 0x800100B4u);          /* main, through its wrapper */
    fprintf(stderr, "JOJO start: main returned\n");
    abort();
}
