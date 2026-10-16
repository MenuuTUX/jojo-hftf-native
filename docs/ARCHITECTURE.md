# Architecture

## Recompiled base

psxrecomp turns the game executable (`disc/SLUS_010.60`) into C (`generated/`) and runs it on a runtime that models the PlayStation hardware: CPU timing, GPU, SPU, CD drive, DMA, timers and the memory card. Overlays, the code modules the game loads from the disc during play, run through the runtime's interpreter or its compiled cache. That gives a game that already plays correctly on macOS.

## Native code on top

The game's own functions are replaced one at a time with native C.

- `src/game/gen_*.c` is generated from Hex-Rays pseudocode of each function. The generated C reaches guest memory only through the interface in `src/game/guest_mem.h`, so it keeps the original memory layout.
- `src/game/manual/` holds functions written by hand from the MIPS where the generated version was wrong. The usual causes are branch delay slots, jump tables, arguments a function passes through in registers it never names, and registers a callee leaves for the next call.
- `src/hybrid/` hooks each native function into the runtime. The executable's functions are replaced at their compiled entry points. Overlay functions are hooked by address, and each hook first checks a CRC of the code at that address. The check matters because all 26 characters load at the same two addresses (slot A at 0x800DF000, slot B at 0x800F4800), so one address can hold a different character's native code depending on who is loaded.
- The PlayStation library calls the game makes (pad, CD, GPU, sound, memory card, kernel threads) have native replacements in `src/hybrid` and `src/platform`.

A function only runs natively after it passed verification. The list of verified functions is compiled into the hooks, and every other function falls back to the recompiled code. Environment variables (see INSTALL.md) turn groups on or off.

## Verification

The development repository does the checking:

1. **Capture.** The game plays a scripted route with every native function off and saves the entry state (registers, RAM, scratchpad, interrupt state) of each hooked function, up to a few dozen times per function.
2. **Replay.** Each saved state is run twice in an isolated copy of the process, once through the recompiled original and once through the native function. Registers, RAM, scratchpad and device writes must match exactly.
3. **Coverage.** A live run with the verified functions on records which code paths the native functions take. Every one of those paths must also have been taken on a state that compared clean.
4. **End to end.** The same input plays with native code on and off, and a per-frame log of game state, including hashes of all RAM pages, is compared.

The routes cover the 22 selectable characters in both player slots, the four encounter-only bosses as the second player, every stage, VS and training mode, and the select screens.

## Timing

The recompiled game keeps the PlayStation's timing: each native function charges the guest clock an estimate of the cycles the original would have used. The vblank clock and the fight start match the original exactly. Where the main loop's occasional lag frames fall can differ, because it depends on per-frame cycle totals to within a few thousand cycles. The final native game will run one game step per displayed frame and have no guest clock at all.
