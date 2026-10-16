# Hybrid-stage glue (development only)

Code in this folder adapts the native platform pieces (`../platform/`) to the PS1 layer of psxrecomp while the game code is still translated. None of it belongs to the final target.

| File | Piece | What it does |
|---|---|---|
| `psx_cd_sector_override.c` | B3, file layer | strong definition of the weak hook `psx_cd_sector_override()`; builds raw CD sectors for form 1 files from the extracted folder |
| `psxrecomp_patches/cdrom_sector_override.patch` | B3 | the only change to the framework copy `JoJoPS1/psxrecomp/runtime/src/cdrom.c`: a weak hook and one call in `read_sector_at`. Revert with `patch -R -p1` inside `JoJoPS1/psxrecomp`. `ps1-port/psxrecomp` (the original clone) is untouched. |

Build wiring: the last block of `JoJoPS1/CMakeLists.txt`. Switches at run time: unset or `JOJO_FILE_LAYER=0` (off, the default: REFERENCE/H0 behavior), `JOJO_FILE_LAYER=1` (serve from the folder), `JOJO_FILE_LAYER=verify` (serve and compare with the disc image), `JOJO_EXTRACTED_DIR`. Comparison runs use no `JOJO_*` variables.
