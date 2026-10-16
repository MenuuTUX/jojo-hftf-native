cmake_minimum_required(VERSION 3.20)
set(PSXRECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/psxrecomp" CACHE PATH
    "Path to psxrecomp")
if(NOT EXISTS "${PSXRECOMP_ROOT}/runtime/runtime.cmake")
    message(FATAL_ERROR
        "PSXRecomp runtime is missing: ${PSXRECOMP_ROOT}/runtime/runtime.cmake\n"
        "This project expects the psxrecomp framework checkout at ${PSXRECOMP_ROOT}.\n"
        "Run: git submodule update --init --recursive\n"
        "If psxrecomp/ is not a submodule, set -DPSXRECOMP_ROOT=<path-to-psxrecomp>.")
endif()

project(JoJoPS1 LANGUAGES C CXX)

# --- Opt-in product surfaces (OFF by default in the framework) -------------
# Scaffold prompts / flags fill these. Without recomp-ui, PSX_RECOMP_UI is OFF.
# The wizard (WIZARD_CMAKE_BLOCK / ENABLE_SETUP_WIZARD) is the first-run disc
# picker; releases are compiled from the committed generated/ C, so it never
# needs to generate or rebuild anything on a player's machine.
set(PSX_RECOMP_UI OFF CACHE BOOL
    "No recomp-ui submodule in this scaffold" FORCE)
# Full netplay UI — enable after adding recomp-ui + testing:
# if(EXISTS "${PSXRECOMP_ROOT}/lib/recomp-net/CMakeLists.txt")
#     set(PSX_NETPLAY ON CACHE BOOL "…" FORCE)
# endif()
# First-run setup wizard — enable after testing:
# set(PSX_SETUP_WIZARD ON CACHE BOOL "…" FORCE)
include("${PSXRECOMP_ROOT}/runtime/runtime.cmake")

# This project's own mod catalog. The FRAMEWORK stages it -- together with its
# builtin psx.* packages -- into <exe-dir>/mods/bundled, and refuses to
# configure if a catalog exists here but is not declared. Never copy it by
# hand: five titles did, and framework 4cc04be3's rename of the staged
# directory broke all five silently (bead beads-eio.3.101).
set(PSX_PRELOADED_MODS "${CMAKE_CURRENT_SOURCE_DIR}/mods/preloaded")
if(NOT IS_DIRECTORY "${PSX_PRELOADED_MODS}")
    set(PSX_PRELOADED_MODS "NONE")
endif()

psxrecomp_add_game_runtime(psx-runtime
    PRELOADED_MODS_DIR "${PSX_PRELOADED_MODS}"
    # ENABLE_NETPLAY_IF_PRESENT
    # NETPLAY_LOBBY_URL "ws://netplay.retcomm.net:8765"
    # ENABLE_SETUP_WIZARD
    WINDOW_TITLE "JoJoPS1"
    DEFAULT_GAME_CONFIG_PATH "game.toml"
    MAX_PLAYERS 2
    # LAUNCHER_BOXART "${CMAKE_CURRENT_SOURCE_DIR}/launcher_assets/img/boxart.tga"
    APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/psxrecomp.ico"
    # Committed game C (psxrecomp_cli.py generate; git add generated). Release
    # CI configures with -DPSXRECOMP_REQUIRE_GAME_C=ON so a checkout without
    # it fails instead of building a setup host.
    GEN_MARKER "generated/SLUS_010.60_dispatch.c"
    GEN_FULL_GLOB "generated/SLUS_010.60_full_*.c"
    # CODEGEN_SETUP_SOURCES — add with recomp-ui / wizard later
)

# Stage runtime configs next to the exe for direct build/ launches. Only copy
# files that exist — never fail POST_BUILD when game_options.toml is absent.
foreach(_PSX_RUNTIME_CONFIG game.toml game_options.toml)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_PSX_RUNTIME_CONFIG}")
        add_custom_command(TARGET psx-runtime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/${_PSX_RUNTIME_CONFIG}"
                "$<TARGET_FILE_DIR:psx-runtime>/${_PSX_RUNTIME_CONFIG}"
            COMMENT "Staging ${_PSX_RUNTIME_CONFIG}"
            VERBATIM)
    endif()
endforeach()

# --- Phase 1 hybrid pieces (development stage only; see ../PHASE1_DESIGN.md) ---------------------------------------
# B3: serve data-file sectors to the PS1 CD controller from the folder made by ../tools-phase1/extract_disc.py.
set(JOJO_PHASE1_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
target_sources(psx-runtime PRIVATE
    "${JOJO_PHASE1_DIR}/src/platform/file_layer.c"
    "${JOJO_PHASE1_DIR}/src/hybrid/psx_cd_sector_override.c"
    "${JOJO_PHASE1_DIR}/src/hybrid/jojo_core.c"
    "${JOJO_PHASE1_DIR}/src/hybrid/jojo_diff.c"
    "${JOJO_PHASE1_DIR}/src/hybrid/jojo_fuzz.c"
    "${JOJO_PHASE1_DIR}/src/hybrid/jojo_ovl.c"
    "${JOJO_PHASE1_DIR}/src/hybrid/jojo_game_hit.c"
    "${JOJO_PHASE1_DIR}/src/game/hit.c"
    "${JOJO_PHASE1_DIR}/src/hybrid/jojo_replay.c"
    "${JOJO_PHASE1_DIR}/src/hybrid/jojo_game_acmain.c"
    "${JOJO_PHASE1_DIR}/src/game/gen_acmain.c"
    "${JOJO_PHASE1_DIR}/src/hybrid/jojo_game_gametr.c"
    "${JOJO_PHASE1_DIR}/src/game/gen_gametr.c"
    "${JOJO_PHASE1_DIR}/src/hybrid/jojo_game_ac_twop.c"
    "${JOJO_PHASE1_DIR}/src/game/gen_ac_twop.c"
    "${JOJO_PHASE1_DIR}/src/hybrid/jojo_game_menu.c"
    "${JOJO_PHASE1_DIR}/src/game/gen_menu.c"
    "${JOJO_PHASE1_DIR}/src/hybrid/jojo_game_tr_sel.c"
    "${JOJO_PHASE1_DIR}/src/game/gen_tr_sel.c"
    "${JOJO_PHASE1_DIR}/src/hybrid/jojo_game_vs_sel.c"
    "${JOJO_PHASE1_DIR}/src/game/gen_vs_sel.c"
    "${JOJO_PHASE1_DIR}/src/hybrid/jojo_game_option.c"
    "${JOJO_PHASE1_DIR}/src/game/gen_option.c"
    "${JOJO_PHASE1_DIR}/src/hybrid/jojo_game_gamevs.c"
    "${JOJO_PHASE1_DIR}/src/game/gen_gamevs.c"
    "${JOJO_PHASE1_DIR}/src/hybrid/jojo_game_gamess.c"
    "${JOJO_PHASE1_DIR}/src/game/gen_gamess.c"
    "${JOJO_PHASE1_DIR}/src/hybrid/jojo_game_ss_vs.c"
    "${JOJO_PHASE1_DIR}/src/game/gen_ss_vs.c")
# Character overlays share slot addresses; generated glue selects by module and function fingerprints.
file(GLOB JOJO_CHARACTER_SOURCES CONFIGURE_DEPENDS
    "${JOJO_PHASE1_DIR}/src/game/gen_pl*.c" "${JOJO_PHASE1_DIR}/src/hybrid/jojo_game_pl*.c")
target_sources(psx-runtime PRIVATE ${JOJO_CHARACTER_SOURCES})
# Hand-written functions of the manual queue (game/manual.json, tools-phase2/merge_manual.py).
file(GLOB_RECURSE JOJO_MANUAL_SOURCES CONFIGURE_DEPENDS "${JOJO_PHASE1_DIR}/src/game/manual/*.c")
if(JOJO_MANUAL_SOURCES)
    target_sources(psx-runtime PRIVATE ${JOJO_MANUAL_SOURCES})
endif()
# Branch coverage of the native game code over replayed states (tools-phase2/coverage.py; separate build dir only, never build-p2).
option(JOJO_COVERAGE "Instrument the native game C (game/) with clang source coverage" OFF)
if(JOJO_COVERAGE)
    file(GLOB _JOJO_COV_SOURCES "${JOJO_PHASE1_DIR}/src/game/gen_*.c")
    list(APPEND _JOJO_COV_SOURCES ${JOJO_MANUAL_SOURCES} "${JOJO_PHASE1_DIR}/src/game/hit.c" "${JOJO_PHASE1_DIR}/src/game/anim_vm.c"
         "${JOJO_PHASE1_DIR}/src/game/anim_handlers.c" "${JOJO_PHASE1_DIR}/src/game/scheduler.c")
    set_property(SOURCE ${_JOJO_COV_SOURCES} TARGET_DIRECTORY psx-runtime APPEND PROPERTY COMPILE_OPTIONS
                 -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(psx-runtime PRIVATE -fprofile-instr-generate)
    target_compile_definitions(psx-runtime PRIVATE JOJO_COVERAGE=1)
endif()
# Coverage-guided capture (hybrid/jojo_sancov.c, tools-phase2/sancov_capture.py; separate build dir build-san only).
option(JOJO_SANCOV "Instrument the native game C with trace-pc-guard and function entry/exit hooks" OFF)
if(JOJO_SANCOV)
    file(GLOB _JOJO_SAN_SOURCES "${JOJO_PHASE1_DIR}/src/game/gen_*.c")
    list(APPEND _JOJO_SAN_SOURCES ${JOJO_MANUAL_SOURCES} "${JOJO_PHASE1_DIR}/src/game/hit.c" "${JOJO_PHASE1_DIR}/src/game/anim_vm.c"
         "${JOJO_PHASE1_DIR}/src/game/anim_handlers.c" "${JOJO_PHASE1_DIR}/src/game/scheduler.c")
    set_property(SOURCE ${_JOJO_SAN_SOURCES} TARGET_DIRECTORY psx-runtime APPEND PROPERTY COMPILE_OPTIONS
                 -fsanitize-coverage=trace-pc-guard -finstrument-functions-after-inlining)
    target_sources(psx-runtime PRIVATE "${JOJO_PHASE1_DIR}/src/hybrid/jojo_sancov.c")
    target_compile_definitions(psx-runtime PRIVATE JOJO_SANCOV=1)
endif()
target_include_directories(psx-runtime PRIVATE "${JOJO_PHASE1_DIR}/src/hybrid")
target_link_libraries(psx-runtime PRIVATE z)
target_include_directories(psx-runtime PRIVATE "${JOJO_PHASE1_DIR}/src/platform")
target_compile_definitions(psx-runtime PRIVATE JOJO_EXTRACTED_DIR_DEFAULT="${JOJO_PHASE1_DIR}/assets/disc"
    JOJO_EXTRACTED_XA_DIR_DEFAULT="${JOJO_PHASE1_DIR}/assets/xa")

# Guest-function replacements (hybrid wrappers). Each family binds its symbols to the wrapper in HLE_SOURCES and keeps the original body as
# jojo_lle_<symbol>; the wrapper calls it unless the piece is switched on (hybrid/jojo_core.h). Same-shard direct callers keep the original:
# tools-phase1/shard_callers.py lists them for every symbol (none for the pad family).
file(GLOB JOJO_SHARDS "${CMAKE_CURRENT_SOURCE_DIR}/generated/SLUS_010.60_full_*.c")
# jojo_select = psxrecomp_select_guest_implementation with GENERATED_SOURCES cut to the shards that define the family's symbols
# (game/shard_map.txt, tools-phase2/shard_map.py). The psxrecomp check scans each given shard once per symbol; passing all 29 shards
# to every family made each configure take several minutes. A symbol missing from the map falls back to all shards.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${JOJO_PHASE1_DIR}/src/game/shard_map.txt")
file(STRINGS "${JOJO_PHASE1_DIR}/src/game/shard_map.txt" _JOJO_SHARD_MAP)
foreach(_row IN LISTS _JOJO_SHARD_MAP)
    string(FIND "${_row}" " " _sp)
    string(SUBSTRING "${_row}" 0 ${_sp} _sym)
    math(EXPR _sp "${_sp} + 1")
    string(SUBSTRING "${_row}" ${_sp} -1 _shard)
    set(JOJO_SHARD_OF_${_sym} "${_shard}")
endforeach()
function(jojo_select)
    cmake_parse_arguments(J "" "NAME;IMPLEMENTATION;SNAPSHOT_TAG;REFERENCE_PREFIX" "TARGETS;GENERATED_SOURCES;HLE_SOURCES;SYMBOLS" ${ARGN})
    set(_src "")
    foreach(_s IN LISTS J_SYMBOLS)
        if(NOT DEFINED JOJO_SHARD_OF_${_s})
            set(_src ${J_GENERATED_SOURCES})
            break()
        endif()
        list(APPEND _src "${CMAKE_CURRENT_SOURCE_DIR}/generated/${JOJO_SHARD_OF_${_s}}")
    endforeach()
    list(REMOVE_DUPLICATES _src)
    psxrecomp_select_guest_implementation(NAME ${J_NAME} IMPLEMENTATION ${J_IMPLEMENTATION} TARGETS ${J_TARGETS}
        GENERATED_SOURCES ${_src} SYMBOLS ${J_SYMBOLS} HLE_SOURCES ${J_HLE_SOURCES}
        REFERENCE_PREFIX ${J_REFERENCE_PREFIX} SNAPSHOT_TAG ${J_SNAPSHOT_TAG})
endfunction()
set(JOJO_TARGETS psx-runtime)
if(TARGET psx-runtime-pgxp)
    list(APPEND JOJO_TARGETS psx-runtime-pgxp)
endif()
target_sources(psx-runtime PRIVATE "${JOJO_PHASE1_DIR}/src/platform/pad.c")
jojo_select(
    NAME jojopad IMPLEMENTATION HLE
    TARGETS ${JOJO_TARGETS}
    GENERATED_SOURCES ${JOJO_SHARDS}
    SYMBOLS func_80010FD8 func_8004FD40 func_8004D8A0 func_8004DCEC func_8004DC6C
    HLE_SOURCES "${JOJO_PHASE1_DIR}/src/hybrid/jojo_pad.c"
    REFERENCE_PREFIX jojo_lle_
    SNAPSHOT_TAG 0x4A4F0001)

# The shared 0x8004D92C guest thunk is owned by CD at the guest entry point.
# Preserve the existing pad implementation as a helper for pad-only runs.
set_source_files_properties("${JOJO_PHASE1_DIR}/src/hybrid/jojo_pad.c"
    PROPERTIES COMPILE_DEFINITIONS "func_8004D92C=jojo_pad_func_8004D92C")

# Phase 1 CD/libcd service. The platform stream stays CPU-independent; the
# hybrid adapter owns the guest callbacks and is selected alongside the pad
# adapter, with the generated libcd bodies retained as the LLE reference.
target_sources(psx-runtime PRIVATE "${JOJO_PHASE1_DIR}/src/platform/cd_stream.c")
jojo_select(
    NAME jojocd IMPLEMENTATION HLE
    TARGETS ${JOJO_TARGETS}
    GENERATED_SOURCES ${JOJO_SHARDS}
    SYMBOLS func_80049370 func_800494B4 func_800495E0 func_80049824
            func_80049BC8 func_80049E4C func_80049EDC func_80049F28
            func_80049F38 func_8004CB00 func_8004CB20 func_8004CB80
            func_8004CC84 func_8004D280 func_8004D304 func_8004D368
            func_8004D820 func_8004D834 func_8004D92C func_8004D9F8
    HLE_SOURCES "${JOJO_PHASE1_DIR}/src/hybrid/jojo_cd.c"
    REFERENCE_PREFIX jojo_lle_
    SNAPSHOT_TAG 0x4A4F00CD)

# Phase 1 saves (libmcrd/libcard/libmcx): platform/save.c owns the files, the adapter keeps the guest-side contract.
target_sources(psx-runtime PRIVATE "${JOJO_PHASE1_DIR}/src/platform/save.c")
jojo_select(
    NAME jojosaves IMPLEMENTATION HLE
    TARGETS ${JOJO_TARGETS}
    GENERATED_SOURCES ${JOJO_SHARDS}
    SYMBOLS func_80044FC0 func_80044FF0 func_80045860 func_800458D0 func_80045910 func_80045C94 func_80045FA4 func_8004612C
            func_80046170 func_800467B0 func_80046C64 func_80046D94 func_800472FC
            func_80050920 func_80050990 func_800510BC func_8005149C func_80052C94 func_80052DC4 func_80053214
    HLE_SOURCES "${JOJO_PHASE1_DIR}/src/hybrid/jojo_saves.c"
    REFERENCE_PREFIX jojo_lle_
    SNAPSHOT_TAG 0x4A4F0053)

# Phase 1 math (libgcc soft float and small libc helpers).
target_sources(psx-runtime PRIVATE "${JOJO_PHASE1_DIR}/src/platform/plat_math.c")
jojo_select(
    NAME jojomath IMPLEMENTATION HLE
    TARGETS ${JOJO_TARGETS}
    GENERATED_SOURCES ${JOJO_SHARDS}
    SYMBOLS func_8003D020 func_8003D1D0 func_8003D200 func_8003D3C0 func_8003D470 func_8003D650
            func_8003D720 func_8003D860 func_8003DDC0 func_80035C10 func_80035C50 func_80035C80
    HLE_SOURCES "${JOJO_PHASE1_DIR}/src/hybrid/jojo_math.c"
    REFERENCE_PREFIX jojo_lle_
    SNAPSHOT_TAG 0x4A4F004D)

# Phase 2 harness control (development only, see ../PHASE2_DESIGN.md): the scheduler 0x80029300 with a control native body.
jojo_select(
    NAME jojodiffctl IMPLEMENTATION HLE
    TARGETS ${JOJO_TARGETS}
    GENERATED_SOURCES ${JOJO_SHARDS}
    SYMBOLS func_80029300
    HLE_SOURCES "${JOJO_PHASE1_DIR}/src/hybrid/jojo_diff_control.c"
    REFERENCE_PREFIX jojo_lle_
    SNAPSHOT_TAG 0x4A4F00D2)

# Phase 2 slice step 1: animation VM. Native game code in ps1-port/game/ (set_anim, tick, advance and 0x80026AFC, which calls advance
# from the same shard); the 21 opcode handlers stay translated. Switch: JOJO_GAME=anim. See ../PHASE2_DESIGN.md.
target_sources(psx-runtime PRIVATE "${JOJO_PHASE1_DIR}/src/game/anim_vm.c" "${JOJO_PHASE1_DIR}/src/game/anim_handlers.c" "${JOJO_PHASE1_DIR}/src/game/scheduler.c")
target_include_directories(psx-runtime PRIVATE "${JOJO_PHASE1_DIR}/src/game")
jojo_select(
    NAME jojoanim IMPLEMENTATION HLE
    TARGETS ${JOJO_TARGETS}
    GENERATED_SOURCES ${JOJO_SHARDS}
    SYMBOLS func_800268BC func_80026A04 func_80026A60 func_80026AFC
    HLE_SOURCES "${JOJO_PHASE1_DIR}/src/hybrid/jojo_game_anim.c"
    REFERENCE_PREFIX jojo_lle_
    SNAPSHOT_TAG 0x4A4F00A1)

# Fourth VM entry (start a stream by index), generated glue.
jojo_select(
    NAME jojoanimidx IMPLEMENTATION HLE
    TARGETS ${JOJO_TARGETS}
    GENERATED_SOURCES ${JOJO_SHARDS}
    SYMBOLS func_80026804
    HLE_SOURCES "${JOJO_PHASE1_DIR}/src/hybrid/jojo_game_animidx.c"
    REFERENCE_PREFIX jojo_lle_
    SNAPSHOT_TAG 0x4A4F00A3)

# The 21 opcode handlers, generated glue (tools-phase2/gen_game_glue.py). Same switch: JOJO_GAME=anim.
jojo_select(
    NAME jojoanimh IMPLEMENTATION HLE
    TARGETS ${JOJO_TARGETS}
    GENERATED_SOURCES ${JOJO_SHARDS}
    SYMBOLS func_80026BA0 func_80026DB8 func_80026DDC func_80026E50 func_80026EC0 func_80026F30 func_80026F50 func_80026F80 func_80026FB8 func_80027008 func_80027040 func_80027084 func_800270B4 func_800270C8 func_80027138 func_80027184 func_800271EC func_8002721C func_8002723C func_8002726C func_80027368
    HLE_SOURCES "${JOJO_PHASE1_DIR}/src/hybrid/jojo_game_animh.c"
    REFERENCE_PREFIX jojo_lle_
    SNAPSHOT_TAG 0x4A4F00A2)

# Exe functions of the slice, machine-translated (tools-phase2/regen_text.sh). The symbol list is generated with the glue, so the
# family follows the generator: game/gen_text.symbols.txt holds "SYMBOLS func_... func_...". Switch: JOJO_GAME=text.
# One family per shard (game/gen_text.shards.txt: "SHARD func_... func_..."), because the psxrecomp family check scans every
# GENERATED_SOURCES file once per symbol: one family over all 29 shards made each configure take about 10 minutes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${JOJO_PHASE1_DIR}/src/game/gen_text.shards.txt")
file(STRINGS "${JOJO_PHASE1_DIR}/src/game/gen_text.shards.txt" _JOJO_TEXT_ROWS)
target_sources(psx-runtime PRIVATE "${JOJO_PHASE1_DIR}/src/game/gen_text.c")
set(_JOJO_TEXT_I 0)
foreach(_row IN LISTS _JOJO_TEXT_ROWS)
    separate_arguments(_row UNIX_COMMAND "${_row}")
    list(POP_FRONT _row _shard)
    math(EXPR _tag "0x4A4F0100 + ${_JOJO_TEXT_I}" OUTPUT_FORMAT HEXADECIMAL)
    psxrecomp_select_guest_implementation(
        NAME jojotext${_JOJO_TEXT_I} IMPLEMENTATION HLE
        TARGETS ${JOJO_TARGETS}
        GENERATED_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/generated/${_shard}"
        SYMBOLS ${_row}
        HLE_SOURCES "${JOJO_PHASE1_DIR}/src/hybrid/jojo_game_text.c"
        REFERENCE_PREFIX jojo_lle_
        SNAPSHOT_TAG ${_tag})
    math(EXPR _JOJO_TEXT_I "${_JOJO_TEXT_I} + 1")
endforeach()

# Native start-up (Phase 2 step 7): start enters main through the dispatcher so the native main can run (hybrid/jojo_start.c).
jojo_select(
    NAME jojostart IMPLEMENTATION HLE
    TARGETS ${JOJO_TARGETS}
    GENERATED_SOURCES ${JOJO_SHARDS}
    SYMBOLS func_8001000C
    HLE_SOURCES "${JOJO_PHASE1_DIR}/src/hybrid/jojo_start.c"
    REFERENCE_PREFIX jojo_lle_
    SNAPSHOT_TAG 0x4A4F00B1)

# Phase 1 GPU family (libgpu + libetc VSync/callback entries). Replaced as a whole, see ../PHASE1_DESIGN.md section 3.
target_sources(psx-runtime PRIVATE
    "${JOJO_PHASE1_DIR}/src/platform/gpu_gp0.c" "${JOJO_PHASE1_DIR}/src/platform/gpu_vram.c" "${JOJO_PHASE1_DIR}/src/platform/gpu_raster.c"
    "${JOJO_PHASE1_DIR}/src/platform/gpu_soft.c")
jojo_select(
    NAME jojogpu IMPLEMENTATION HLE
    TARGETS ${JOJO_TARGETS}
    GENERATED_SOURCES ${JOJO_SHARDS}
    SYMBOLS func_80035D20 func_80036004 func_8003609C func_800362B0 func_80036348 func_800364C0 func_80036588 func_80036634 func_80036690 func_80036700 func_800368CC func_800389C0 func_800396F0 func_800397B0 func_800397F0 func_80039830 func_80039850 func_80039890 func_800398C0 func_800398E0 func_80039900 func_80039920 func_80039940 func_80039960 func_80039980 func_800399B0 func_800399F0 func_8003C4A0 func_8003C594
    HLE_SOURCES "${JOJO_PHASE1_DIR}/src/hybrid/jojo_gpu.c"
    REFERENCE_PREFIX jojo_lle_
    SNAPSHOT_TAG 0x4A4F0047)

# Phase 1 libgte (18 of 19 entries; InitGeom 0x80039C08 stays on the translated body, see PHASE1_DESIGN.md section 9).
target_sources(psx-runtime PRIVATE "${JOJO_PHASE1_DIR}/src/platform/plat_gte.c")
jojo_select(
    NAME jojogte IMPLEMENTATION HLE
    TARGETS ${JOJO_TARGETS}
    GENERATED_SOURCES ${JOJO_SHARDS}
    SYMBOLS func_80039A90 func_80039B60 func_80039C90 func_80039DC0 func_80039FD0 func_8003A070 func_8003A1B0 func_8003A2F0
            func_8003A380 func_8003A3C0 func_8003A3E0 func_8003A714 func_8003A7E0 func_8003A810 func_8003A840 func_8003A970
            func_8003AC00 func_8003BFA0
    HLE_SOURCES "${JOJO_PHASE1_DIR}/src/hybrid/jojo_gte.c"
    REFERENCE_PREFIX jojo_lle_
    SNAPSHOT_TAG 0x4A4F0054)

# Phase 1 audio (libsnd/libspu: 22 bound entries). The sequence player (platform/audio_seq.c) is not linked: the game opens no sequences.
target_sources(psx-runtime PRIVATE
    "${JOJO_PHASE1_DIR}/src/platform/audio_adpcm.c" "${JOJO_PHASE1_DIR}/src/platform/audio_vab.c" "${JOJO_PHASE1_DIR}/src/platform/audio_mix.c"
    "${JOJO_PHASE1_DIR}/src/platform/audio_xa.c")
jojo_select(
    NAME jojoaudio IMPLEMENTATION HLE
    TARGETS ${JOJO_TARGETS}
    GENERATED_SOURCES ${JOJO_SHARDS}
    SYMBOLS func_8003DEC0 func_8003DF70 func_8003E050 func_8003E070 func_8003E130 func_8003E3B0
            func_8003F4C0 func_8003F5D0 func_8003F940 func_8003FA90 func_8003FDEC func_8003FFAC
            func_80041EE0 func_80041EF0 func_80041F00 func_80041F10 func_80041FC0 func_80042420
            func_800424E0 func_80042670 func_800440B0 func_80044200
    HLE_SOURCES "${JOJO_PHASE1_DIR}/src/hybrid/jojo_audio.c"
    REFERENCE_PREFIX jojo_lle_
    SNAPSHOT_TAG 0x4A4F0041)

# Phase 1 kernel (step 7): the seven exe stubs 0x80035A30..0x80035A90. Native: SetSp, EnterCriticalSection, ExitCriticalSection;
# InitHeap and the thread calls (OpenTh, CloseTh, ChangeTh) take the original body. See hybrid/jojo_kernel.c.
jojo_select(
    NAME jojokernel IMPLEMENTATION HLE
    TARGETS ${JOJO_TARGETS}
    GENERATED_SOURCES ${JOJO_SHARDS}
    SYMBOLS func_80035A30 func_80035A40 func_80035A50 func_80035A60 func_80035A70 func_80035A80 func_80035A90
    HLE_SOURCES "${JOJO_PHASE1_DIR}/src/hybrid/jojo_kernel.c"
    REFERENCE_PREFIX jojo_lle_
    SNAPSHOT_TAG 0x4A4F004B)

# --- JoJo shell library: the Swift app hosts the runtime in-process (shell/README.md, "Hosted runtime") ---------------
# OFF by default, so no other build dir changes. ON (build-shell only): main.cpp is compiled with main renamed to
# psxrecomp_main (hybrid/jojo_host_entry.c calls it); jojo_pad.c and jojo_audio.c have their vblank and pull entry points
# renamed (*_impl), so the strong wrappers in hybrid/jojo_host_entry.c take their place with no edit to those files; and
# hybrid/jojo_host_main.c keeps the psx-runtime executable linkable. shell/build-host.sh links the object files of
# psx-runtime except jojo_host_main.c.o into the app.
option(JOJO_SHELL_LIB "Build the runtime for the Swift shell (renamed main, host pad and audio wrappers)" OFF)
if(JOJO_SHELL_LIB)
    get_source_file_property(_jojo_main_defs "${PSXRECOMP_ROOT}/runtime/src/main.cpp" COMPILE_DEFINITIONS)
    if(_jojo_main_defs STREQUAL "NOTFOUND")
        set(_jojo_main_defs "")
    endif()
    set_source_files_properties("${PSXRECOMP_ROOT}/runtime/src/main.cpp" PROPERTIES
        COMPILE_DEFINITIONS "${_jojo_main_defs};main=psxrecomp_main")

    get_source_file_property(_jojo_pad_defs "${JOJO_PHASE1_DIR}/src/hybrid/jojo_pad.c" COMPILE_DEFINITIONS)
    if(_jojo_pad_defs STREQUAL "NOTFOUND")
        set(_jojo_pad_defs "")
    endif()
    set_source_files_properties("${JOJO_PHASE1_DIR}/src/hybrid/jojo_pad.c" PROPERTIES
        COMPILE_DEFINITIONS "${_jojo_pad_defs};jojo_pad_frame=jojo_pad_frame_impl")

    get_source_file_property(_jojo_audio_defs "${JOJO_PHASE1_DIR}/src/hybrid/jojo_audio.c" COMPILE_DEFINITIONS)
    if(_jojo_audio_defs STREQUAL "NOTFOUND")
        set(_jojo_audio_defs "")
    endif()
    set_source_files_properties("${JOJO_PHASE1_DIR}/src/hybrid/jojo_audio.c" PROPERTIES
        COMPILE_DEFINITIONS "${_jojo_audio_defs};jojo_audio_frame=jojo_audio_frame_impl;jojo_audio_pull=jojo_audio_pull_impl")

    target_sources(psx-runtime PRIVATE
        "${JOJO_PHASE1_DIR}/src/hybrid/jojo_host_entry.c"
        "${JOJO_PHASE1_DIR}/src/hybrid/jojo_host_main.c")
endif()
