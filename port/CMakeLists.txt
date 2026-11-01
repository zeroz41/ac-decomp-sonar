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
#   MSL's C library, except its math, which the game keeps (porting-model.md, Floating-point determinism);
#   the NES emulator, out of scope until its assembly is converted (roadmap M6);
#   the console's hardware set-up, which the host has no counterpart for: the memory map and the DSP tasks that
#   unlock the memory card and answer the Game Boy Advance;
#   the start-up that loads foresta.rel and runs its static constructors, which the port's entry point does.
set(replaced_sources
    "^src/static/dolphin/"
    "^src/static/TRK_MINNOW_DOLPHIN/"
    "^src/static/Runtime\\.PPCEABI\\.H/"
    "^src/static/Famicom/"
    "^src/static/libforest/ReconfigBATs\\.c$"
    "^src/static/jaudio_NES/internal/dsp_(cardunlock|GBAKey)\\.c$"
    "^src/static/boot\\.c$"
    "^src/executor\\.c$")
set(msl_math "^src/static/MSL_C\\.PPCEABI\\.bare\\.H/(e|k|s|w)_[a-z0-9_]+\\.c$")
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
    if(NOT replaced)
        list(APPEND units "${unit}")
    endif()
endforeach()
list(LENGTH units unit_count)
message(STATUS "Animal Crossing: ${unit_count} units")

add_library(ac_game OBJECT)
sonar_decomp_add_units(ac_game PREFIX AC ROOT "${AC_ROOT}" UNITS ${units} DROP_DEFINES MUST_MATCH)
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
                       $<$<COMPILE_LANGUAGE:C>:-Werror=pointer-to-int-cast>
                       # C++ treats falling off the end of a function as unreachable, not as an unused result.
                       $<$<COMPILE_LANGUAGE:CXX>:-Werror=return-type>)
# Units that relocate the audio driver's bank and wave data in place, turning its 32-bit offsets into pointers; they
# need a loader that builds the host's structures.
set(narrow_pointer_sources
    jaudio_NES/internal/bankread.c jaudio_NES/internal/oneshot.c jaudio_NES/internal/system.c
    jaudio_NES/internal/waveread.c)
list(TRANSFORM narrow_pointer_sources PREPEND "${AC_ROOT}/src/static/")
set_source_files_properties(${narrow_pointer_sources} TARGET_DIRECTORY ac_game
                            PROPERTIES COMPILE_OPTIONS -Wno-error=int-to-pointer-cast)
# The original compiler took C++98, which allowed narrowing in braced initializers.
target_compile_options(ac_game PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-narrowing>)
target_link_libraries(ac_game PRIVATE sonar::base)

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
