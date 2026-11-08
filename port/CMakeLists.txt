# Animal Crossing's native port (sonarlib docs/porting-model.md): the decompilation's game code built for the host and
# linked against sonarlib. Configure the decompilation first (python configure.py, then ninja), which writes
# objdiff.json and the asset includes under build/<version>.
cmake_minimum_required(VERSION 3.25)
project(ac_port LANGUAGES C CXX)
# GX's C99 inline __GXReadCPCounterU32 (include/dolphin/gx/GXRegs.h) has no external definition without optimization,
# so a build with no type set fails to link: a single-config generator defaults to RelWithDebInfo.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "The build type" FORCE)
endif()
# The seam audit (ac_seam_audit) reads the translation units the build compiles from compile_commands.json.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(AC_VERSION "GAFE01_00" CACHE STRING "The disc revision to build: GAFE01_00 or GAFU01_00")
set(SONARLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../sonarlib" CACHE PATH "sonarlib's source tree")

get_filename_component(AC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(AC_BUILD "${AC_ROOT}/build/${AC_VERSION}")
if(NOT EXISTS "${AC_BUILD}/include/assets")
    message(FATAL_ERROR "No assets in ${AC_BUILD}/include; build the decompilation for ${AC_VERSION} first")
endif()
# The boot sound's bank and wave set, which the decompilation's build extracts since sonarlib's M6 step 6m.
set(boot_bank "${AC_BUILD}/bin/assets/jaudio/NIN_BANK.bin")
set(boot_wave "${AC_BUILD}/bin/assets/jaudio/NIN_WAVE.bin")
if(NOT EXISTS "${boot_bank}" OR NOT EXISTS "${boot_wave}")
    message(FATAL_ERROR "No boot sound in ${AC_BUILD}/bin/assets/jaudio; build the decompilation for ${AC_VERSION} again")
endif()

# The importer is built with sonarlib's tools: sl-schema and sonar::import.
set(SONAR_BUILD_TOOLS ON)
add_subdirectory("${SONARLIB_DIR}" sonarlib EXCLUDE_FROM_ALL)

sonar_decomp_read_units(PREFIX AC OBJDIFF "${AC_ROOT}/objdiff.json")

# The game's C and C++ units, without the libraries sonarlib and the host replace:
#   the Dolphin SDK, the debugger stub and the compiler runtime;
#   MSL's C library, except its math and the NaN and infinity it returns, which the game keeps (porting-model.md,
#   Floating-point determinism);
#   the NES emulator's core and drawing, out of scope until the core's assembly is converted (roadmap M6): the port
#   keeps the game's Famicom front end, whose famicom_init declines to start, and stubs the core's entry points;
#   the console's hardware set-up, which the host has no counterpart for: the memory map and the DSP tasks that
#   unlock the memory card and answer the Game Boy Advance;
#   foresta.rel's start-up, which runs its static constructors: the port links foresta in (boot.c's LoadLink).
# emu64 stays, under other names (below): the game's calls of it run on sonarlib's kit (kits/emu64, roadmap M6 step
# 6g) through the glue (adapter/emu64_glue.c), and --diff runs it beside the kit as the reference (step 6j).
set(replaced_sources
    "^src/static/dolphin/"
    "^src/static/TRK_MINNOW_DOLPHIN/"
    "^src/static/Runtime\\.PPCEABI\\.H/"
    "^src/static/Famicom/ks_nes_(core|draw)\\.cpp$"
    "^src/static/libforest/ReconfigBATs\\.c$"
    "^src/static/jaudio_NES/internal/dsp_(cardunlock|GBAKey)\\.c$"
    "^src/executor\\.c$")
set(msl_math "^src/static/MSL_C\\.PPCEABI\\.bare\\.H/((e|k|s|w)_[a-z0-9_]+|float)\\.c$")
set(units "")
foreach(unit IN LISTS AC_UNITS)
    set(source "${AC_UNIT_${unit}_SOURCE}")
    if(NOT AC_UNIT_${unit}_LANGUAGE MATCHES "^(C|CXX)$")
        continue()
    endif()
    set(replaced FALSE)
    foreach(pattern IN LISTS replaced_sources)
        if(source MATCHES "${pattern}")
            set(replaced TRUE)
        endif()
    endforeach()
    if(source MATCHES "^src/static/MSL_C\\.PPCEABI\\.bare\\.H/" AND NOT source MATCHES "${msl_math}")
        set(replaced TRUE)
    endif()
    # The SDK's calendar arithmetic, which touches no hardware; its matrix library, whose paired-single assembly has
    # C fallbacks on sonarlib's paired singles; the pad's clamping, which is arithmetic; and its GX units, which write
    # into the glue's sink (adapter/dolphin_gx.c), except the FIFO's, which the glue replaces.
    if(source MATCHES "^src/static/dolphin/(os/OSTime|mtx/(mtx|mtx44|mtxvec|vec)|pad/Padclamp|gx/GX[A-Za-z]+)\\.c$"
       AND NOT source STREQUAL "src/static/dolphin/gx/GXFifo.c")
        set(replaced FALSE)
    endif()
    if(NOT replaced)
        list(APPEND units "${unit}")
    endif()
endforeach()
list(LENGTH units unit_count)
message(STATUS "Animal Crossing: ${unit_count} units")

# What reads the game's headers as the game does: its include directories, the port's switch and the adapter's forced
# includes. The game and the importer both build on it.
add_library(ac_headers INTERFACE)
target_include_directories(ac_headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/adapter/include"
    "${AC_ROOT}/include" "${AC_ROOT}/src/static/dolphin"
    "${AC_BUILD}/include" "${AC_BUILD}/bin/assets"
    # The original compiler found quoted includes from the repository root too.
    "${AC_ROOT}")
target_compile_definitions(ac_headers INTERFACE SONAR_PORT=1)
if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(adapter_includes "/FI${CMAKE_CURRENT_SOURCE_DIR}/adapter/intrinsics.h"
        "/FI${CMAKE_CURRENT_SOURCE_DIR}/adapter/names.h")
else()
    set(adapter_includes "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/adapter/intrinsics.h"
        "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/adapter/names.h")
endif()
target_compile_options(ac_headers INTERFACE ${adapter_includes})

add_library(ac_game OBJECT)
sonar_decomp_add_units(ac_game PREFIX AC ROOT "${AC_ROOT}" UNITS ${units} DROP_DEFINES MUST_MATCH)
# The SDK's GX units that the console's build does not list (objdiff.json), built as its listed GX units are: the
# immediate-mode writers, the shapes, the display-list saver and the verifier, the last three under DEBUG only.
set(gx_unlisted_sources "")
foreach(name IN ITEMS GXDraw GXSave GXVerifRAS GXVerifXF GXVerify GXVert)
    list(APPEND gx_unlisted_sources "${AC_ROOT}/src/static/dolphin/gx/${name}.c")
endforeach()
get_source_file_property(gx_defines "${AC_ROOT}/src/static/dolphin/gx/GXAttr.c" COMPILE_DEFINITIONS)
get_source_file_property(gx_options "${AC_ROOT}/src/static/dolphin/gx/GXAttr.c" COMPILE_OPTIONS)
set_source_files_properties(${gx_unlisted_sources} PROPERTIES COMPILE_DEFINITIONS "${gx_defines}"
                            COMPILE_OPTIONS "${gx_options}")
target_sources(ac_game PRIVATE ${gx_unlisted_sources})
# The boot's main and foresta's, which the port links into one program.
set_property(SOURCE "${AC_ROOT}/src/static/boot.c" APPEND PROPERTY COMPILE_DEFINITIONS main=ac_boot_main)
set_property(SOURCE "${AC_ROOT}/src/main.c" APPEND PROPERTY COMPILE_DEFINITIONS main=ac_foresta_main)
# emu64, the --diff run's reference (roadmap M6 step 6j): its C entry points and FrameCansel take the emu64_ref_ names,
# as the glue keeps the game's; the macros rename the class's members of the same names too, which nothing else sees.
# It keeps what the console's compiler accepted: fastcast_float's register, which C++17 refuses; string literals as
# char* in its name tables, panic's parameters and warningString; and combine_auto's comparison of an alpha argument
# with GX_CC_ZERO (the decompilation's @BUG).
set(emu64_ref_names "")
foreach(name IN ITEMS init cleanup taskstart set_ucode_info set_first_ucode refresh texture_cache_data_entry_set
                      set_aflags get_aflags)
    list(APPEND emu64_ref_names "emu64_${name}=emu64_ref_${name}")
endforeach()
list(APPEND emu64_ref_names FrameCansel=emu64_ref_FrameCansel)
set_property(SOURCE "${AC_ROOT}/src/static/libforest/emu64/emu64.c" APPEND PROPERTY COMPILE_DEFINITIONS
             ${emu64_ref_names})
set_property(SOURCE "${AC_ROOT}/src/static/libforest/emu64/emu64.c" APPEND PROPERTY COMPILE_OPTIONS -Wno-register
             -Wno-write-strings -Wno-enum-compare)
# The one definition of each of MSL's math inlines (include/MSL_C/w_math.h).
target_sources(ac_game PRIVATE adapter/msl_inlines.c)
set_source_files_properties(adapter/msl_inlines.c PROPERTIES COMPILE_DEFINITIONS AC_PORT_MSL_INLINES)
# The SDK's OS functions on sonarlib's kit.
target_sources(ac_game PRIVATE adapter/dolphin_os.c)
# The SDK's ARAM and its DMA queue.
target_sources(ac_game PRIVATE adapter/dolphin_aram.c)
# The SDK's Game Boy Advance functions, with none connected.
target_sources(ac_game PRIVATE adapter/dolphin_gba.c)
# The SDK's memory cards on sonarlib's virtual card.
target_sources(ac_game PRIVATE adapter/dolphin_card.c)
# The SDK's audio interface on sonarlib audio's frame, and the DSP behind its mailbox.
target_sources(ac_game PRIVATE adapter/dolphin_ai.c adapter/dolphin_dsp.c)
# The NES emulator's entry points, which halt: its core is excluded below.
target_sources(ac_game PRIVATE adapter/famicom_core.cpp)
set_source_files_properties(adapter/famicom_core.cpp PROPERTIES COMPILE_DEFINITIONS "F3DEX_GBI_2;_LANGUAGE_C")
# The SDK's DVD functions on the package mounted as game:, for the disc revision this build is for.
target_sources(ac_game PRIVATE adapter/dolphin_dvd.c)
set_source_files_properties(adapter/dolphin_dvd.c PROPERTIES COMPILE_DEFINITIONS
                            "AC_PORT_DISC_VERSION=\"${AC_VERSION}\";F3DEX_GBI_2;_LANGUAGE_C")
# The SDK's video and controller functions on sonarlib's VI and PAD kits.
target_sources(ac_game PRIVATE adapter/dolphin_vi.c adapter/dolphin_pad.c)
# The sink the SDK's GX units write into, handing its bytes to sonarlib's GX front end, GX's address space, the frame
# from JW_BeginFrame to JW_EndFrame, and the FIFO functions of GXFifo.c, which it replaces.
target_sources(ac_game PRIVATE adapter/dolphin_gx.c)
# What the game calls of emu64, on sonarlib's kit, and the proof's stop at play's first frame.
target_sources(ac_game PRIVATE adapter/emu64_glue.c)
set_source_files_properties(adapter/emu64_glue.c PROPERTIES COMPILE_DEFINITIONS "F3DEX_GBI_2;_LANGUAGE_C")
# The out-of-line copies of the game's C99 inlines.
target_sources(ac_game PRIVATE adapter/game_inlines.c)
set_source_files_properties(adapter/game_inlines.c PROPERTIES COMPILE_DEFINITIONS "F3DEX_GBI_2;_LANGUAGE_C")
# JSystem's system font, which the game keeps in its executable (JUTFontData_Ascfont_fix12.s): the decompilation's
# build extracts it, and ac-font converts it to the host's byte order for the adapter to include, as JUTResFont reads
# its integers in place.
add_executable(ac-font importer/font.cpp importer/font_main.cpp)
set_target_properties(ac-font PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS ON)
target_link_libraries(ac-font PRIVATE sonar::base sonar::core)
set(system_font "${AC_BUILD}/bin/assets/JSystem/JUtility/FontData/Ascfont_fix12.bfn")
set(system_font_include "${CMAKE_CURRENT_BINARY_DIR}/assets/Ascfont_fix12.inc")
add_custom_command(OUTPUT "${system_font_include}"
                   COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets"
                   COMMAND ac-font "${system_font}" "${system_font_include}"
                   DEPENDS ac-font "${system_font}"
                   COMMENT "Converting the system font"
                   VERBATIM)
target_sources(ac_game PRIVATE adapter/jsystem_font.c "${system_font_include}")
set_source_files_properties(adapter/jsystem_font.c PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/assets"
                            OBJECT_DEPENDS "${system_font_include}")
# The boot sound's bank and wave set (NIN_BANK and NIN_WAVE, game64_dummy.c_inc), which JAudio1 reads in place as
# structures of pointers: ac-boot-sound lays both out for the host on sonarlib's jaudio kit (NOTES.md, Boot sound).
add_executable(ac-boot-sound importer/boot_sound.cpp importer/boot_sound_main.cpp)
set_target_properties(ac-boot-sound PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS ON)
target_link_libraries(ac-boot-sound PRIVATE ac_headers sonar::base sonar::core sonar::kit_jaudio)
set(boot_sound_include "${CMAKE_CURRENT_BINARY_DIR}/assets/boot_sound.inc")
add_custom_command(OUTPUT "${boot_sound_include}"
                   COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets"
                   COMMAND ac-boot-sound "${boot_bank}" "${boot_wave}" "${boot_sound_include}"
                   DEPENDS ac-boot-sound "${boot_bank}" "${boot_wave}"
                   COMMENT "Laying out the boot sound"
                   VERBATIM)
target_sources(ac_game PRIVATE "${boot_sound_include}")
set_property(SOURCE "${AC_ROOT}/src/static/jaudio_NES/game/game64.c" APPEND
             PROPERTY INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/assets")
set_property(SOURCE "${AC_ROOT}/src/static/jaudio_NES/game/game64.c" APPEND PROPERTY OBJECT_DEPENDS "${boot_sound_include}")
# The decompilation's libc headers stand in for MSL's; the port takes the host's (porting-model.md, Floating-point
# determinism, for the math library).
set_target_properties(ac_game PROPERTIES C_STANDARD 11 C_EXTENSIONS ON CXX_STANDARD 20 CXX_EXTENSIONS ON)
# The exact-sim profile (porting-model.md, 64-bit and endianness; Floating-point determinism).
target_compile_options(ac_game PRIVATE -fwrapv -fno-strict-aliasing -fno-delete-null-pointer-checks
                       -ffp-contract=off -Werror=int-to-pointer-cast
                       $<$<COMPILE_LANGUAGE:C>:-Werror=pointer-to-int-cast> -Werror=return-type
                       # C++ treats falling off the end of a function as unreachable, not as an unused result.
                       $<$<COMPILE_LANGUAGE:CXX>:-Werror=return-type>)
# GCC calls a virtual function's likely override directly, emitting the inline overrides of classes the game never
# links, such as JUTRomFont's.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ac_game PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-devirtualize-speculatively>)
endif()
# The original compiler took C++98, which allowed narrowing in braced initializers.
target_compile_options(ac_game PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-narrowing>)
target_link_libraries(ac_game PRIVATE ac_headers sonar::base sonar::core sonar::kit_dolphin_os sonar::kit_card
                      sonar::kit_jaudio sonar::kit_vi sonar::kit_pad sonar::kit_emu64 sonar::gx_frontend sonar::fmt_card)

# The views of the game's card files (NOTES.md, Saves): ac_saves.schema's converters and what ac_saves.c converts around
# them, built with m_card.c's macros and char signedness. They are built without debug information, so the port's DWARF
# that ac_save_schema_check reads holds the game's own definitions of the two saved structures the game defines in its
# units, not ac_port.h's copies of them.
add_library(ac_saves OBJECT adapter/ac_saves.c)
sonar_add_schema(ac_saves adapter/ac_saves.schema PREFIX ac_saves
                 INCLUDES m_common_data.h m_card.h m_event.h ac_aprilfool_control_h.h ac_hatumode_control.h
                          ac_npc_totakeke.h ac_set_npc_manager.h ac_ev_angler.h ac_tokyoso_control.h
                          ac_tunahiki_control.h ac_ev_turkey.h ac_ev_yomise.h ac_port.h)
get_source_file_property(card_defines "${AC_ROOT}/src/game/m_card.c" COMPILE_DEFINITIONS)
get_source_file_property(card_options "${AC_ROOT}/src/game/m_card.c" COMPILE_OPTIONS)
target_include_directories(ac_saves PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/adapter")
target_compile_definitions(ac_saves PRIVATE ${card_defines} AC_SAVES_TYPES)
target_compile_options(ac_saves PRIVATE ${card_options} -fwrapv -fno-strict-aliasing -fno-delete-null-pointer-checks)
if(NOT CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(ac_saves PRIVATE -g0)
endif()
set_target_properties(ac_saves PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_link_libraries(ac_saves PRIVATE ac_headers sonar::base sonar::kit_card)

# The program (NOTES.md, The program): the game and its glue, from boot to play's first frame.
add_executable(ac_port adapter/main.c $<TARGET_OBJECTS:ac_game> $<TARGET_OBJECTS:ac_saves>)
set_target_properties(ac_port PROPERTIES C_STANDARD 11 C_EXTENSIONS ON LINKER_LANGUAGE CXX)
target_link_libraries(ac_port PRIVATE ac_headers sonar::base sonar::core sonar::kit_dolphin_os sonar::kit_card
                      sonar::kit_jaudio sonar::kit_vi sonar::kit_pad sonar::kit_emu64 sonar::gx_frontend sonar::fmt_card)
# sonarlib's sl-card, which moves the cards' saves to and from Dolphin (README.md, Saves), is built with the program.
add_dependencies(ac_port sl-card)

# The importer (NOTES.md, The package): ac-import writes the package the game reads from boot to Town's first frame
# from the player's disc, converting the structures the game reads in place (sonarlib docs/roadmap.md, M6 step 6c).
# sonarlib's jaudio kit lays out audiorom.img from the game's own tables (audioheaders.c).
add_library(ac_import_lib STATIC importer/archive.cpp importer/audio.cpp importer/import.cpp
            ${AC_ROOT}/src/static/jaudio_NES/game/audioheaders.c)
sonar_add_schema(ac_import_lib importer/ac.schema PREFIX ac LANGUAGE CXX
                 INCLUDES JSystem/JKernel/JKRArchive.h m_field_make.h jsyswrap.h)
# m_field_make.h needs the display-list switches the game's units take from configure.py's flags (cflags_common).
target_compile_definitions(ac_import_lib PUBLIC F3DEX_GBI_2 _LANGUAGE_C)
set_target_properties(ac_import_lib PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS ON)
target_link_libraries(ac_import_lib PUBLIC ac_headers sonar::import sonar::base sonar::kit_jaudio)
add_executable(ac-import importer/main.cpp)
set_target_properties(ac-import PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS ON)
target_link_libraries(ac-import PRIVATE ac_import_lib sonar::core)

# The port's views of a display-list command (tools/gbi_views.py): each field where the console's compiler put it,
# and the views current with the console's structs.
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(ac_gbi_views_test tests/gbi_views_test.c)
target_include_directories(ac_gbi_views_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/adapter/include"
    "${AC_ROOT}/include" "${AC_ROOT}/src/static/dolphin" "${AC_ROOT}")
target_compile_definitions(ac_gbi_views_test PRIVATE SONAR_PORT=1 F3DEX_GBI_2 _LANGUAGE_C)
target_compile_options(ac_gbi_views_test PRIVATE ${adapter_includes})
set_target_properties(ac_gbi_views_test PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_link_libraries(ac_gbi_views_test PRIVATE sonar::base)
add_test(NAME ac_gbi_views COMMAND ac_gbi_views_test)
add_test(NAME ac_gbi_views_current COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/gbi_views.py" --check)

# The audio driver's command: each byte where the console read it from a big-endian word, and a whole host address.
add_executable(ac_audio_port_test tests/audio_port_test.c)
target_include_directories(ac_audio_port_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/adapter/include"
    "${AC_ROOT}/include" "${AC_ROOT}/src/static/dolphin" "${AC_ROOT}")
target_compile_definitions(ac_audio_port_test PRIVATE SONAR_PORT=1 F3DEX_GBI_2 _LANGUAGE_C)
target_compile_options(ac_audio_port_test PRIVATE ${adapter_includes})
set_target_properties(ac_audio_port_test PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_link_libraries(ac_audio_port_test PRIVATE sonar::base)
add_test(NAME ac_audio_port COMMAND ac_audio_port_test)

# The bit-field views of a whole value: each field on the bits the console's compiler gave it.
add_executable(ac_bitfield_views_test tests/bitfield_views_test.c)
target_include_directories(ac_bitfield_views_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/adapter/include"
    "${AC_ROOT}/include" "${AC_ROOT}/src/static/dolphin" "${AC_ROOT}")
target_compile_definitions(ac_bitfield_views_test PRIVATE SONAR_PORT=1 F3DEX_GBI_2 _LANGUAGE_C)
target_compile_options(ac_bitfield_views_test PRIVATE ${adapter_includes})
set_target_properties(ac_bitfield_views_test PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_link_libraries(ac_bitfield_views_test PRIVATE sonar::base)
add_test(NAME ac_bitfield_views COMMAND ac_bitfield_views_test)

# The SDK's matrix library on the port's paired-single fallbacks: the matrices and vectors its assembly computes.
# The library's units are built as ac_game builds them, each source keeping its unit's char signedness and macros, with
# the game's names and intrinsics; the test's own file takes neither, since its reference math is the host's.
set(mtx_sources "")
foreach(name IN ITEMS mtx mtx44 mtxvec vec)
    list(APPEND mtx_sources "${AC_ROOT}/src/static/dolphin/mtx/${name}.c")
endforeach()
add_library(ac_mtx_units OBJECT ${mtx_sources})
add_executable(ac_mtx_port_test tests/mtx_port_test.c $<TARGET_OBJECTS:ac_mtx_units>)
foreach(target IN ITEMS ac_mtx_units ac_mtx_port_test)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/adapter/include"
        "${AC_ROOT}/include" "${AC_ROOT}/src/static/dolphin" "${AC_ROOT}")
    target_compile_definitions(${target} PRIVATE SONAR_PORT=1 DEBUG=0 VERSION=0)
    target_compile_options(${target} PRIVATE -ffp-contract=off)
    set_target_properties(${target} PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
endforeach()
target_compile_options(ac_mtx_units PRIVATE ${adapter_includes} -fwrapv -fno-strict-aliasing
                       -fno-delete-null-pointer-checks)
target_link_libraries(ac_mtx_units PRIVATE sonar::base)
target_link_libraries(ac_mtx_port_test PRIVATE sonar::base m)
add_test(NAME ac_mtx_port COMMAND ac_mtx_port_test)

# Game units built as ac_game builds them, each source keeping its unit's char signedness and macros, with the stubs
# their other references need: lbRTC_IsEqualDate's date order, libultra's formatter of doubles through libc64's
# sprintf, and the pad display's colors.
foreach(name IN ITEMS rtc printf hayakawa)
    add_executable(ac_${name}_port_test tests/${name}_port_test.c)
    target_include_directories(ac_${name}_port_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/adapter/include"
        "${AC_ROOT}/include" "${AC_ROOT}/src/static/dolphin" "${AC_ROOT}")
    target_compile_definitions(ac_${name}_port_test PRIVATE SONAR_PORT=1 DEBUG=0 VERSION=0 F3DEX_GBI_2 _LANGUAGE_C)
    target_compile_options(ac_${name}_port_test PRIVATE ${adapter_includes} -fwrapv -fno-strict-aliasing
                           -fno-delete-null-pointer-checks -ffp-contract=off)
    set_target_properties(ac_${name}_port_test PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
    target_link_libraries(ac_${name}_port_test PRIVATE sonar::base m)
    add_test(NAME ac_${name}_port COMMAND ac_${name}_port_test)
endforeach()
target_sources(ac_rtc_port_test PRIVATE "${AC_ROOT}/src/lb_rtc.c")
target_sources(ac_printf_port_test PRIVATE "${AC_ROOT}/src/static/libultra/xprintf.c"
    "${AC_ROOT}/src/static/libultra/xldtob.c" "${AC_ROOT}/src/static/libultra/xlitob.c"
    "${AC_ROOT}/src/static/libc64/sprintf.c")
target_sources(ac_hayakawa_port_test PRIVATE "${AC_ROOT}/src/game/m_debug_hayakawa.c")


# The SDK's ARAM and its DMA queue on core os.
add_executable(ac_aram_port_test tests/aram_port_test.c adapter/dolphin_aram.c)
target_include_directories(ac_aram_port_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/adapter/include" "${AC_ROOT}/include"
    "${AC_ROOT}/src/static/dolphin" "${AC_ROOT}")
target_compile_definitions(ac_aram_port_test PRIVATE SONAR_PORT=1)
target_compile_options(ac_aram_port_test PRIVATE ${adapter_includes})
set_target_properties(ac_aram_port_test PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_link_libraries(ac_aram_port_test PRIVATE sonar::core)
add_test(NAME ac_aram_port COMMAND ac_aram_port_test)


# The SDK's memory cards on sonarlib's virtual card in a memory store, with the views of the game's files.
add_executable(ac_card_port_test tests/card_port_test.c adapter/dolphin_card.c $<TARGET_OBJECTS:ac_saves>)
target_include_directories(ac_card_port_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/adapter"
    "${CMAKE_CURRENT_SOURCE_DIR}/adapter/include" "${AC_ROOT}/include" "${AC_ROOT}/src/static/dolphin" "${AC_ROOT}")
target_compile_definitions(ac_card_port_test PRIVATE SONAR_PORT=1)
target_compile_options(ac_card_port_test PRIVATE ${adapter_includes})
set_target_properties(ac_card_port_test PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_link_libraries(ac_card_port_test PRIVATE sonar::core sonar::kit_card)
add_test(NAME ac_card_port COMMAND ac_card_port_test)

# The views of the game's card files, each case worked by hand in the console's bytes, built as ac_saves is.
add_executable(ac_save_views_test tests/save_views_test.c $<TARGET_OBJECTS:ac_saves>)
target_include_directories(ac_save_views_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/adapter")
target_compile_definitions(ac_save_views_test PRIVATE ${card_defines} AC_SAVES_TYPES)
target_compile_options(ac_save_views_test PRIVATE ${card_options} -fwrapv -fno-strict-aliasing)
set_target_properties(ac_save_views_test PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_link_libraries(ac_save_views_test PRIVATE ac_headers sonar::base sonar::kit_card)
add_test(NAME ac_save_views COMMAND ac_save_views_test)

# ac_saves.schema's coverage of the game's saved types, against the port's own DWARF: the event save areas' data is
# ac_saves.c's.
sonar_add_schema_check(ac_save_schema_check adapter/ac_saves.schema TARGET ac_port PORT_CONVERTS mEv_area_c.data)

# The SDK's audio interface on sonarlib audio's frame, and the DSP answering jaudio's own boot and commands and
# rendering its channels from ARAM.
set(jaudio_dsp_sources "")
foreach(name IN ITEMS dspboot dspproc dspinterface)
    list(APPEND jaudio_dsp_sources "${AC_ROOT}/src/static/jaudio_NES/internal/${name}.c")
endforeach()
add_executable(ac_ai_dsp_port_test tests/ai_dsp_port_test.c adapter/dolphin_ai.c adapter/dolphin_dsp.c
    adapter/dolphin_aram.c ${jaudio_dsp_sources})
# jaudio is C++, as the game builds it, and so the test that calls it.
set_source_files_properties(tests/ai_dsp_port_test.c ${jaudio_dsp_sources} PROPERTIES LANGUAGE CXX)
target_include_directories(ac_ai_dsp_port_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/adapter"
    "${CMAKE_CURRENT_SOURCE_DIR}/adapter/include" "${AC_ROOT}/include" "${AC_ROOT}/src/static/dolphin" "${AC_ROOT}")
target_compile_definitions(ac_ai_dsp_port_test PRIVATE SONAR_PORT=1 F3DEX_GBI_2 _LANGUAGE_C)
target_compile_options(ac_ai_dsp_port_test PRIVATE ${adapter_includes})
set_target_properties(ac_ai_dsp_port_test PROPERTIES C_STANDARD 11 C_EXTENSIONS ON CXX_STANDARD 20 CXX_EXTENSIONS ON)
target_link_libraries(ac_ai_dsp_port_test PRIVATE sonar::core sonar::kit_jaudio)
add_test(NAME ac_ai_dsp_port COMMAND ac_ai_dsp_port_test)

# The importer's archive conversion on a console archive built by hand.
add_executable(ac_import_archive_test tests/import_archive_test.cpp)
set_target_properties(ac_import_archive_test PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS ON)
target_link_libraries(ac_import_archive_test PRIVATE ac_import_lib)
add_test(NAME ac_import_archive COMMAND ac_import_archive_test)

# ac-import on the player's disc, an image or an extracted directory, and ac_port's refusal of a package from before
# the importer's version; skipped (77) without one.
set(AC_DISC "" CACHE PATH "The player's disc for ac_import_disc: an image, or a directory in sl-disc's extracted layout")
add_executable(ac_import_disc_test tests/import_disc_test.cpp)
set_target_properties(ac_import_disc_test PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS ON)
target_link_libraries(ac_import_disc_test PRIVATE ac_import_lib sonar::core sonar::fmt_disc)
add_dependencies(ac_import_disc_test ac-import ac_port)
add_test(NAME ac_import_disc COMMAND ac_import_disc_test "$<TARGET_FILE:ac-import>" "${AC_DISC}" "${AC_VERSION}"
                                     "$<TARGET_FILE:ac_port>")
set_tests_properties(ac_import_disc PROPERTIES SKIP_RETURN_CODE 77)

# ac-font's conversion on a font built by hand.
add_executable(ac_font_test tests/font_test.cpp importer/font.cpp)
set_target_properties(ac_font_test PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS ON)
target_link_libraries(ac_font_test PRIVATE sonar::base)
add_test(NAME ac_font COMMAND ac_font_test)

# ac-boot-sound's images of the decompilation's extracts, walked through the game's own structures against the
# console's bytes, and the build's boot_sound.inc written again.
add_executable(ac_boot_sound_test tests/boot_sound_test.cpp importer/boot_sound.cpp)
set_target_properties(ac_boot_sound_test PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS ON)
target_link_libraries(ac_boot_sound_test PRIVATE ac_headers sonar::base sonar::kit_jaudio)
add_dependencies(ac_boot_sound_test ac-boot-sound)
add_test(NAME ac_boot_sound COMMAND ac_boot_sound_test "${boot_bank}" "${boot_wave}" "${boot_sound_include}")

# ac_port from boot to play's first frame, twice and once with --validate, on a package ac-import writes from the
# player's disc; skipped without one, when the script says so.
add_test(NAME ac_port_play
         COMMAND "${CMAKE_COMMAND}" "-DAC_IMPORT=$<TARGET_FILE:ac-import>" "-DAC_PORT=$<TARGET_FILE:ac_port>"
                 "-DAC_DISC=${AC_DISC}" "-DAC_VERSION=${AC_VERSION}" "-DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/play"
                 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/play_test.cmake")
set_tests_properties(ac_port_play PROPERTIES SKIP_REGULAR_EXPRESSION "ac_port_play: skipped, AC_DISC is not set")

# ac_port --diff, each emu64 task drawn by the kit and by the console's emu64 and the two compared, on its own package
# from the player's disc; skipped without one, when the script says so.
add_test(NAME ac_port_diff
         COMMAND "${CMAKE_COMMAND}" "-DAC_IMPORT=$<TARGET_FILE:ac-import>" "-DAC_PORT=$<TARGET_FILE:ac_port>"
                 "-DAC_DISC=${AC_DISC}" "-DAC_VERSION=${AC_VERSION}" "-DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/diff"
                 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/diff_test.cmake")
set_tests_properties(ac_port_diff PROPERTIES SKIP_REGULAR_EXPRESSION "ac_port_diff: skipped, AC_DISC is not set")

# ac_port --window, a settings round trip at --ticks 1800 that keeps the GPU sink's pipeline list, a broken settings
# file at --ticks 120 that warms from it, and bad --set assignments, with the user directories under OUT_DIR/user, on
# SDL's offscreen video driver and dummy audio driver, with lavapipe's Vulkan, so nothing opens on the desktop, nothing
# plays and no player's file is read or written; skipped without the disc or lavapipe, when the script says so.
find_file(AC_LAVAPIPE_ICD NAMES lvp_icd.x86_64.json lvp_icd.json PATHS /usr/share/vulkan/icd.d NO_DEFAULT_PATH)
if(NOT AC_LAVAPIPE_ICD)
    set(AC_LAVAPIPE_ICD "")
endif()
add_test(NAME ac_port_window
         COMMAND "${CMAKE_COMMAND}" "-DAC_IMPORT=$<TARGET_FILE:ac-import>" "-DAC_PORT=$<TARGET_FILE:ac_port>"
                 "-DAC_DISC=${AC_DISC}" "-DAC_VERSION=${AC_VERSION}" "-DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/window"
                 "-DAC_LAVAPIPE_ICD=${AC_LAVAPIPE_ICD}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/window_test.cmake")
set_tests_properties(ac_port_window PROPERTIES SKIP_REGULAR_EXPRESSION "ac_port_window: skipped, ")

# ac_port --reference to play's first frame, and --measure --ticks 1200 with the kit and with --reference twice each,
# then with the kit at --present-hz 144 and 360 once each, on the GPU sink with no window through SDL's offscreen video
# driver and lavapipe's Vulkan; skipped without the disc or lavapipe, when the script says so.
add_test(NAME ac_port_measure
         COMMAND "${CMAKE_COMMAND}" "-DAC_IMPORT=$<TARGET_FILE:ac-import>" "-DAC_PORT=$<TARGET_FILE:ac_port>"
                 "-DAC_DISC=${AC_DISC}" "-DAC_VERSION=${AC_VERSION}" "-DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/measure"
                 "-DAC_LAVAPIPE_ICD=${AC_LAVAPIPE_ICD}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/measure_test.cmake")
set_tests_properties(ac_port_measure PROPERTIES SKIP_REGULAR_EXPRESSION "ac_port_measure: skipped, ")

# The goldens: sonarlib's sl-capture runs Dolphin on the player's disc and caches its frames in the build tree, ac_port
# --goldens writes the same scenes on the GPU sink with no window, offscreen through lavapipe, and sl-capture compares
# them; skipped without the disc, Dolphin, lavapipe or sl-capture, when the script says so.
set(AC_DOLPHIN "" CACHE FILEPATH "dolphin-emu-nogui, built from upstream's tag 2609, for ac_port_goldens")
if(TARGET sl-capture)
    add_custom_target(ac_capture_tool ALL DEPENDS sl-capture)
    set(ac_sl_capture "$<TARGET_FILE:sl-capture>")
else()
    set(ac_sl_capture "")
endif()
add_test(NAME ac_port_goldens
         COMMAND "${CMAKE_COMMAND}" "-DAC_IMPORT=$<TARGET_FILE:ac-import>" "-DAC_PORT=$<TARGET_FILE:ac_port>"
                 "-DSL_CAPTURE=${ac_sl_capture}" "-DAC_DOLPHIN=${AC_DOLPHIN}" "-DAC_DISC=${AC_DISC}"
                 "-DAC_VERSION=${AC_VERSION}" "-DSCENES=${CMAKE_CURRENT_SOURCE_DIR}/tests/goldens.scenes"
                 "-DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/goldens" "-DAC_LAVAPIPE_ICD=${AC_LAVAPIPE_ICD}"
                 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/goldens_test.cmake")
set_tests_properties(ac_port_goldens PROPERTIES SKIP_REGULAR_EXPRESSION "ac_port_goldens: skipped, ")

# The seam audit (sonarlib's sl-seam-audit) over the translation units this build compiles, against the port's policy
# of seam files (seam-policy.toml): every file that uses GX is listed, and no transitional entry is stale. No disc.
add_custom_target(ac_seam_audit_tool ALL DEPENDS sl-seam-audit)
add_test(NAME ac_seam_audit
         COMMAND sl-seam-audit --policy "${CMAKE_CURRENT_SOURCE_DIR}/seam-policy.toml"
                 --commands "${CMAKE_BINARY_DIR}/compile_commands.json" --root "${AC_ROOT}")
