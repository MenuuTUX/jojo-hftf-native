/* Thread exit and jump close the current thread before ChangeTh. They cannot
 * use GuestMem.call4: it waits for a return to an artificial guest address in
 * a thread that will never resume. Keep the original frame and continuations,
 * and hand each kernel call back to the dispatcher instead. */
#ifndef JOJO_THREAD_TRAPS_H
#define JOJO_THREAD_TRAPS_H

#include "jojo_game_glue.h"

static inline void jojo_thread_trap(CPUState *cpu, uint32_t addr) {
    uint32_t jump = addr == 0x800109F0u;
    uint32_t enter_ret = addr + (jump ? 36u : 28u);
    uint32_t close_ret = addr + (jump ? 56u : 48u);
    uint32_t exit_ret = addr + (jump ? 64u : 56u);
    uint32_t change_ret = addr + (jump ? 72u : 64u);
    uint32_t pc = cpu->pc, p;

    if (pc == enter_ret) {
        cpu->gpr[2] = cpu->read_word(0x80066294u);
        cpu->gpr[4] = cpu->read_word(cpu->gpr[2] + 4u);
        cpu->gpr[31] = close_ret;
        cpu->pc = 0x80035A50u;
    } else if (pc == close_ret) {
        cpu->gpr[31] = exit_ret;
        cpu->pc = 0x80035A80u;
    } else if (pc == exit_ret) {
        cpu->gpr[4] = 0xFF000000u;
        cpu->gpr[31] = change_ret;
        cpu->pc = 0x80035A60u;
    } else if (pc == change_ret) {
        cpu->gpr[31] = cpu->read_word(cpu->gpr[29] + 20u);
        cpu->gpr[16] = cpu->read_word(cpu->gpr[29] + 16u);
        cpu->gpr[29] += 24u;
        jojo_return(cpu);
    } else {
        cpu->gpr[29] -= 24u;
        cpu->write_word(cpu->gpr[29] + 16u, cpu->gpr[16]);
        cpu->gpr[16] = 0x80060000u;
        p = cpu->read_word(0x80066294u);
        if (jump) { cpu->gpr[3] = p; cpu->gpr[2] = 3u; }
        else cpu->gpr[2] = p;
        cpu->write_word(cpu->gpr[29] + 20u, cpu->gpr[31]);
        cpu->write_half(p, jump ? 3u : 0u);
        if (jump) cpu->write_word(p + 12u, cpu->gpr[4]);
        cpu->gpr[31] = enter_ret;
        cpu->pc = 0x80035A70u;
    }
}

#endif
