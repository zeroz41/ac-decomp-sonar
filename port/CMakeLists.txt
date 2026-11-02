# Animal Crossing's native port (sonarlib docs/porting-model.md): the decompilation's game code built for the host and
# linked against sonarlib. Configure the decompilation first (python configure.py, then ninja), which writes
# objdiff.json and the asset includes under build/<version>.
cmake_minimum_required(VERSION 3.25)
project(ac_port LANGUAGES C CXX)

set(AC_VERSION "GAFE01_00" CACHE STRING "The disc revision to build: GAFE01_00 or GAFU01_00")
set(SONARLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../sonarlib" CACHE PATH "sonarlib's source tree")

get_filename_component(AC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(AC_BUILD "${AC_ROOT}/build/${AC_VERSION}")
if(NOT EXISTS "${AC_BUILD}/include/assets")
    message(FATAL_ERROR "No assets in ${AC_BUILD}/include; build the decompilation for ${AC_VERSION} first")
endif()

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
    # The SDK's calendar arithmetic, which touches no hardware, and its matrix library, whose paired-single
    # assembly has C fallbacks on sonarlib's paired singles.
    if(source MATCHES "^src/static/dolphin/(os/OSTime|mtx/(mtx|mtx44|mtxvec|vec))\\.c$")
        set(replaced FALSE)
    endif()
    if(NOT replaced)
        list(APPEND units "${unit}")
    endif()
endforeach()
list(LENGTH units unit_count)
message(STATUS "Animal Crossing: ${unit_count} units")

add_library(ac_game OBJECT)
sonar_decomp_add_units(ac_game PREFIX AC ROOT "${AC_ROOT}" UNITS ${units} DROP_DEFINES MUST_MATCH)
# The boot's main and foresta's, which the port links into one program.
set_property(SOURCE "${AC_ROOT}/src/static/boot.c" APPEND PROPERTY COMPILE_DEFINITIONS main=ac_boot_main)
set_property(SOURCE "${AC_ROOT}/src/main.c" APPEND PROPERTY COMPILE_DEFINITIONS main=ac_foresta_main)
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
# JSystem's system font, which the game keeps in its executable (JUTFontData_Ascfont_fix12.s): the decompilation's
# build extracts it, and this writes its bytes for the adapter to include.
set(system_font "${AC_BUILD}/bin/assets/JSystem/JUtility/FontData/Ascfont_fix12.bfn")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${system_font}")
file(READ "${system_font}" system_font_hex HEX)
string(REGEX REPLACE "(..)" "0x\\1," system_font_bytes "${system_font_hex}")
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/assets/Ascfont_fix12.inc" CONTENT "${system_font_bytes}\n" @ONLY)
target_sources(ac_game PRIVATE adapter/jsystem_font.c)
set_source_files_properties(adapter/jsystem_font.c PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/assets")
target_include_directories(ac_game PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/adapter/include"
    "${AC_ROOT}/include" "${AC_ROOT}/src/static/dolphin"
    "${AC_BUILD}/include" "${AC_BUILD}/bin/assets"
    # The original compiler found quoted includes from the repository root too.
    "${AC_ROOT}")
# The decompilation's libc headers stand in for MSL's; the port takes the host's (porting-model.md, Floating-point
# determinism, for the math library).
set_target_properties(ac_game PROPERTIES C_STANDARD 11 C_EXTENSIONS ON CXX_STANDARD 20 CXX_EXTENSIONS ON)
target_compile_definitions(ac_game PRIVATE SONAR_PORT=1)
if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(adapter_includes "/FI${CMAKE_CURRENT_SOURCE_DIR}/adapter/intrinsics.h"
        "/FI${CMAKE_CURRENT_SOURCE_DIR}/adapter/names.h")
else()
    set(adapter_includes "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/adapter/intrinsics.h"
        "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/adapter/names.h")
endif()
target_compile_options(ac_game PRIVATE ${adapter_includes})
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
target_link_libraries(ac_game PRIVATE sonar::base sonar::core sonar::kit_dolphin_os sonar::kit_card)

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
set(mtx_sources "")
foreach(name IN ITEMS mtx mtx44 mtxvec vec)
    list(APPEND mtx_sources "${AC_ROOT}/src/static/dolphin/mtx/${name}.c")
endforeach()
add_executable(ac_mtx_port_test tests/mtx_port_test.c ${mtx_sources})
target_include_directories(ac_mtx_port_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/adapter/include"
    "${AC_ROOT}/include" "${AC_ROOT}/src/static/dolphin" "${AC_ROOT}")
target_compile_definitions(ac_mtx_port_test PRIVATE SONAR_PORT=1 DEBUG=0 VERSION=0)
# The game's names and intrinsics in the library's units only: the test's reference math is the host's.
if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(mtx_adapter_includes ${adapter_includes})
else()
    # A source's COMPILE_OPTIONS takes no SHELL: groups.
    set(mtx_adapter_includes -include "${CMAKE_CURRENT_SOURCE_DIR}/adapter/intrinsics.h"
        -include "${CMAKE_CURRENT_SOURCE_DIR}/adapter/names.h")
endif()
set_source_files_properties(${mtx_sources} PROPERTIES COMPILE_OPTIONS "${mtx_adapter_includes}")
target_compile_options(ac_mtx_port_test PRIVATE -ffp-contract=off)
set_target_properties(ac_mtx_port_test PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_link_libraries(ac_mtx_port_test PRIVATE sonar::base m)
add_test(NAME ac_mtx_port COMMAND ac_mtx_port_test)


# The SDK's ARAM and its DMA queue on core os.
add_executable(ac_aram_port_test tests/aram_port_test.c adapter/dolphin_aram.c)
target_include_directories(ac_aram_port_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/adapter/include" "${AC_ROOT}/include"
    "${AC_ROOT}/src/static/dolphin" "${AC_ROOT}")
target_compile_definitions(ac_aram_port_test PRIVATE SONAR_PORT=1)
target_compile_options(ac_aram_port_test PRIVATE ${adapter_includes})
set_target_properties(ac_aram_port_test PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_link_libraries(ac_aram_port_test PRIVATE sonar::core)
add_test(NAME ac_aram_port COMMAND ac_aram_port_test)


# The SDK's memory cards on sonarlib's virtual card in a memory store.
add_executable(ac_card_port_test tests/card_port_test.c adapter/dolphin_card.c)
target_include_directories(ac_card_port_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/adapter"
    "${CMAKE_CURRENT_SOURCE_DIR}/adapter/include" "${AC_ROOT}/include" "${AC_ROOT}/src/static/dolphin" "${AC_ROOT}")
target_compile_definitions(ac_card_port_test PRIVATE SONAR_PORT=1)
target_compile_options(ac_card_port_test PRIVATE ${adapter_includes})
set_target_properties(ac_card_port_test PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_link_libraries(ac_card_port_test PRIVATE sonar::core sonar::kit_card)
add_test(NAME ac_card_port COMMAND ac_card_port_test)

# The SDK's audio interface on sonarlib audio's frame, and the DSP answering jaudio's own boot and commands.
add_executable(ac_ai_dsp_port_test tests/ai_dsp_port_test.c adapter/dolphin_ai.c adapter/dolphin_dsp.c
    "${AC_ROOT}/src/static/jaudio_NES/internal/dspboot.c" "${AC_ROOT}/src/static/jaudio_NES/internal/dspproc.c")
# jaudio is C++, as the game builds it, and so the test that calls it.
set_source_files_properties(tests/ai_dsp_port_test.c "${AC_ROOT}/src/static/jaudio_NES/internal/dspboot.c"
    "${AC_ROOT}/src/static/jaudio_NES/internal/dspproc.c" PROPERTIES LANGUAGE CXX)
target_include_directories(ac_ai_dsp_port_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/adapter"
    "${CMAKE_CURRENT_SOURCE_DIR}/adapter/include" "${AC_ROOT}/include" "${AC_ROOT}/src/static/dolphin" "${AC_ROOT}")
target_compile_definitions(ac_ai_dsp_port_test PRIVATE SONAR_PORT=1)
target_compile_options(ac_ai_dsp_port_test PRIVATE ${adapter_includes})
set_target_properties(ac_ai_dsp_port_test PROPERTIES C_STANDARD 11 C_EXTENSIONS ON CXX_STANDARD 20 CXX_EXTENSIONS ON)
target_link_libraries(ac_ai_dsp_port_test PRIVATE sonar::core)
add_test(NAME ac_ai_dsp_port COMMAND ac_ai_dsp_port_test)
