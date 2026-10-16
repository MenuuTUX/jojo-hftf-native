/* Guest memory access for game code in the hybrid stage (Phase 2). Game code (ps1-port/game/*.c) sees only this interface;
 * the hybrid glue (ps1-port/hybrid/jojo_game_*.c) fills it from the emulated CPU. Plain C99, no PS1 or psxrecomp types.
 *
 * Addresses are guest addresses (0x80xxxxxx for main RAM). Reads and writes use the host's byte order only through these
 * functions, so game code never does pointer arithmetic on guest memory. */
#ifndef GUEST_MEM_H
#define GUEST_MEM_H

#include <stdint.h>

typedef struct GuestMem {
    void *ctx;
    uint8_t  (*rd8)(void *ctx, uint32_t addr);
    uint16_t (*rd16)(void *ctx, uint32_t addr);
    uint32_t (*rd32)(void *ctx, uint32_t addr);
    void     (*wr8)(void *ctx, uint32_t addr, uint8_t v);
    void     (*wr16)(void *ctx, uint32_t addr, uint16_t v);
    void     (*wr32)(void *ctx, uint32_t addr, uint32_t v);
    /* Calls a guest function with one argument in $a0 and returns $v0. The callee runs to completion (guest call path). */
    uint32_t (*call1)(void *ctx, uint32_t fn, uint32_t a0);
    /* Same with two arguments ($a0, $a1). */
    uint32_t (*call2)(void *ctx, uint32_t fn, uint32_t a0, uint32_t a1);
    /* Same with four arguments ($a0..$a3); unused ones are passed as 0. */
    uint32_t (*call4)(void *ctx, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
    /* Same as call4, but use the supplied guest $ra. Needed when a callee
     * saves a MIPS continuation (for example, a thread switch). */
    uint32_t (*call4pc)(void *ctx, uint32_t fn, uint32_t return_pc,
                        uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
    /* Read or write a guest GPR. Register 0 always reads as zero and ignores writes. */
    uint32_t (*gpr_get)(void *ctx, uint32_t reg);
    void     (*gpr_set)(void *ctx, uint32_t reg, uint32_t value);
    /* Call through a function pointer: as call4, with $v0 = fn at entry, as the game's `jalr $v0` leaves it (an empty callee
     * then returns its own address, which callers can see). */
    uint32_t (*calli)(void *ctx, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
    /* Five arguments: the fifth goes to the outgoing argument slot at sp+16 of a 32-byte frame below the current $sp, as the
     * original caller's frame holds it. */
    uint32_t (*call5)(void *ctx, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
    /* The guest $sp. A function with locals in memory (arrays, locals whose address is taken) lowers it by its frame size for
     * the body and restores it, as the original's prologue and epilogue do, so guest callees never overwrite those locals. */
    uint32_t (*sp_get)(void *ctx);
    void     (*sp_set)(void *ctx, uint32_t sp);
    /* The value guest register `reg` held when the most recent call (call1/2/4/4pc/calli/call5) returned, before the caller's
     * registers were restored. MIPS code sometimes passes a register a callee left (for example $a2/$a3 written by 800258F0)
     * on to the next call; read it here, never through gpr_get (which sees the caller's registers again). */
    uint32_t (*exit_reg)(void *ctx, uint32_t reg);
} GuestMem;

#endif
