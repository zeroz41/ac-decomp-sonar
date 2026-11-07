# Runs ac_port --window --ticks 1800 (sonarlib docs/roadmap.md, M6 steps 6k, 6l and 6m):
#   cmake -DAC_IMPORT=<ac-import> -DAC_PORT=<ac_port> -DAC_DISC=<disc> -DAC_VERSION=<version> -DOUT_DIR=<dir>
#         -DAC_LAVAPIPE_ICD=<lavapipe's ICD> -P window_test.cmake
# ac-import writes the package from the player's disc into OUT_DIR, then the run shows the game in real time in a window
# on SDL's offscreen video driver, through lavapipe's Vulkan, with SDL's dummy audio driver: nothing opens on the
# desktop and the game's sound is mixed but not heard. It must exit 0 with one play line and a window line with at least
# the limit's ticks (the main loop checks the limit once a presented frame) and frames presented, and the game's sound
# must have mixed loud frames, the title's music, and the DSP rendered sounding frames, the boot jingle. It prints both
# lines. When AC_DISC names no disc or there is no lavapipe it prints the line the test's SKIP_REGULAR_EXPRESSION
# matches and returns.
cmake_minimum_required(VERSION 3.25)

if("${AC_DISC}" STREQUAL "")
    message("ac_port_window: skipped, AC_DISC is not set")
    return()
endif()
if("${AC_LAVAPIPE_ICD}" STREQUAL "")
    message("ac_port_window: skipped, no lavapipe ICD for the offscreen driver's Vulkan surface")
    return()
endif()
foreach(variable IN ITEMS AC_IMPORT AC_PORT AC_VERSION OUT_DIR)
    if("${${variable}}" STREQUAL "")
        message(FATAL_ERROR "ac_port_window: ${variable} is not set")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUT_DIR}")
set(package "${OUT_DIR}/${AC_VERSION}.slpack")
execute_process(COMMAND "${AC_IMPORT}" "${AC_DISC}" "${package}" "${OUT_DIR}/cache"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ac_port_window: ac-import failed (${result}):\n${output}${errors}")
endif()
message("ac-import: ${output}")

set(limit 1800)
string(TIMESTAMP started "%s")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy
                        "VK_DRIVER_FILES=${AC_LAVAPIPE_ICD}" "${AC_PORT}" --package "${package}" --window
                        --ticks ${limit}
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 600)
string(TIMESTAMP ended "%s")
math(EXPR seconds "${ended} - ${started}")
string(REGEX MATCHALL "ac_port: play [^\n]*" plays "${output}")
string(REGEX MATCH "ac_port: window ticks ([0-9]+) frames ([0-9]+) audio ([0-9]+) loud ([0-9]+) dsp ([0-9]+)" line "${output}")
set(ticks "${CMAKE_MATCH_1}")
set(frames "${CMAKE_MATCH_2}")
set(loud "${CMAKE_MATCH_4}")
set(dsp "${CMAKE_MATCH_5}")
list(LENGTH plays count)
message("exit ${result}, ${seconds} s:\n${plays}\n${line}")
if(NOT result EQUAL 0 OR NOT count EQUAL 1 OR line STREQUAL "")
    string(LENGTH "${errors}" length)
    if(length GREATER 4000)
        math(EXPR from "${length} - 4000")
        string(SUBSTRING "${errors}" ${from} -1 errors)
    endif()
    message(FATAL_ERROR "ac_port_window: the run did not reach play's first frame and end at the limit:\n${errors}")
endif()
if(ticks LESS limit OR NOT frames GREATER 0)
    message(FATAL_ERROR "ac_port_window: the window line has fewer ticks than the limit or no frame presented:\n${line}")
endif()
if(NOT loud GREATER 0)
    message(FATAL_ERROR "ac_port_window: the game's sound mixed no loud frame:\n${line}")
endif()
if(NOT dsp GREATER 0)
    message(FATAL_ERROR "ac_port_window: the DSP rendered no sounding frame, so the boot jingle is silent:\n${line}")
endif()
