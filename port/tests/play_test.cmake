# Runs ac_port from boot to play's first frame, twice and once with --validate (sonarlib docs/roadmap.md, M6 steps 6f,
# 6g, 6h, 6l, 6m and 9b):
#   cmake -DAC_IMPORT=<ac-import> -DAC_PORT=<ac_port> -DAC_DISC=<disc> -DAC_VERSION=<version> -DOUT_DIR=<dir>
#         -P play_test.cmake
# ac-import writes the package from the player's disc into OUT_DIR, then each run must reach play's first frame, exit 0
# and print the same ac_port line, so the frame recorded on emu64's kit matches run to run and no static list on the
# path was drawn stale. The frame must hold instances and a hash, with no invalid frame, view overflow or render
# overflow; the kit's refusals and strays and the render module's problems are reported as they are. The GX glue's
# counts must all be 0: no GX error, no unmapped address, no task outside a frame, no nested frame and no refused
# frame. The game's sound must have mixed output frames, the same in each run with the same hash (loud frames come
# after play's first frame), and the DSP must have rendered sounding frames: the boot jingle. When AC_DISC names no
# disc it prints the line the test's SKIP_REGULAR_EXPRESSION matches and returns.
cmake_minimum_required(VERSION 3.25)

if("${AC_DISC}" STREQUAL "")
    message("ac_port_play: skipped, AC_DISC is not set")
    return()
endif()
foreach(variable IN ITEMS AC_IMPORT AC_PORT AC_VERSION OUT_DIR)
    if("${${variable}}" STREQUAL "")
        message(FATAL_ERROR "ac_port_play: ${variable} is not set")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUT_DIR}")
set(package "${OUT_DIR}/${AC_VERSION}.slpack")
execute_process(COMMAND "${AC_IMPORT}" "${AC_DISC}" "${package}" "${OUT_DIR}/cache"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ac_port_play: ac-import failed (${result}):\n${output}${errors}")
endif()
message("ac-import: ${output}")

set(lines "")
set(count "[0-9]+")
set(line_pattern "ac_port: [a-z]+ tick ${count} retrace ${count} gx ${count} dvd ${count} ${count} hash [0-9a-f]+")
string(APPEND line_pattern " instances ${count} compiles ${count} holes ${count}/${count} cpu ${count}")
string(APPEND line_pattern " refused ${count} views ${count} invalid ${count} problems ${count} overflows ${count}")
string(APPEND line_pattern " unowned ${count} strays ${count} gx-errors ${count} unmapped ${count} outside ${count}")
string(APPEND line_pattern " nested ${count} refused-frames ${count} audio ${count} loud ${count} dsp ${count}")
string(APPEND line_pattern " sound [0-9a-f]+")
foreach(run IN ITEMS 1 2 validate)
    set(options "")
    if(run STREQUAL "validate")
        set(options --validate)
    endif()
    string(TIMESTAMP started "%s")
    execute_process(COMMAND "${AC_PORT}" --package "${package}" ${options}
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 1200)
    string(TIMESTAMP ended "%s")
    math(EXPR seconds "${ended} - ${started}")
    string(REGEX MATCH "${line_pattern}" line "${output}")
    message("run ${run}: exit ${result}, ${seconds} s: ${line}")
    if(NOT result EQUAL 0 OR NOT line MATCHES "^ac_port: play ")
        string(LENGTH "${errors}" length)
        if(length GREATER 4000)
            math(EXPR from "${length} - 4000")
            string(SUBSTRING "${errors}" ${from} -1 errors)
        endif()
        message(FATAL_ERROR "ac_port_play: run ${run} did not reach play's first frame:\n${errors}")
    endif()
    list(APPEND lines "${line}")
endforeach()
list(GET lines 0 first)
foreach(other IN LISTS lines)
    if(NOT other STREQUAL first)
        message(FATAL_ERROR "ac_port_play: the runs differ:\n${lines}")
    endif()
endforeach()
string(REGEX MATCH " hash ([0-9a-f]+) instances ([0-9]+) " ignored "${first}")
set(hash "${CMAKE_MATCH_1}")
set(instances "${CMAKE_MATCH_2}")
string(REGEX MATCH " views ([0-9]+) invalid ([0-9]+) problems [0-9]+ overflows ([0-9]+) " ignored "${first}")
set(views "${CMAKE_MATCH_1}")
set(invalid "${CMAKE_MATCH_2}")
set(overflows "${CMAKE_MATCH_3}")
if(instances EQUAL 0 OR hash MATCHES "^0+$")
    message(FATAL_ERROR "ac_port_play: play's first frame has no instances or no hash:\n${first}")
endif()
if(NOT views EQUAL 0 OR NOT invalid EQUAL 0 OR NOT overflows EQUAL 0)
    message(FATAL_ERROR "ac_port_play: play's first frame overflowed or was invalid:\n${first}")
endif()
string(REGEX MATCH
       " gx-errors ([0-9]+) unmapped ([0-9]+) outside ([0-9]+) nested ([0-9]+) refused-frames ([0-9]+) audio ([0-9]+) "
       ignored "${first}")
set(gx_counts "${CMAKE_MATCH_1};${CMAKE_MATCH_2};${CMAKE_MATCH_3};${CMAKE_MATCH_4};${CMAKE_MATCH_5}")
set(gx_names "gx-errors;unmapped;outside;nested;refused-frames")
foreach(value name IN ZIP_LISTS gx_counts gx_names)
    if(NOT value EQUAL 0)
        message(FATAL_ERROR "ac_port_play: the GX glue's ${name} is not 0:\n${first}")
    endif()
endforeach()
if(NOT CMAKE_MATCH_6 GREATER 0)
    message(FATAL_ERROR "ac_port_play: the game's sound mixed no output frames:\n${first}")
endif()
string(REGEX MATCH " dsp ([0-9]+) " ignored "${first}")
if(NOT CMAKE_MATCH_1 GREATER 0)
    message(FATAL_ERROR "ac_port_play: the DSP rendered no sounding frame, so the boot jingle is silent:\n${first}")
endif()
