# Runs ac_port from boot to play's first frame, twice (sonarlib docs/roadmap.md, M6 step 6f):
#   cmake -DAC_IMPORT=<ac-import> -DAC_PORT=<ac_port> -DAC_DISC=<disc> -DAC_VERSION=<version> -DOUT_DIR=<dir>
#         -P play_test.cmake
# ac-import writes the package from the player's disc into OUT_DIR, then each run must reach play's first frame, exit
# 0 and print the same ac_port line. When AC_DISC names no disc it prints the line the test's SKIP_REGULAR_EXPRESSION
# matches and returns.
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
foreach(run IN ITEMS 1 2)
    string(TIMESTAMP started "%s")
    execute_process(COMMAND "${AC_PORT}" --package "${package}"
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 1200)
    string(TIMESTAMP ended "%s")
    math(EXPR seconds "${ended} - ${started}")
    string(REGEX MATCH "ac_port: [a-z]+ tick [0-9]+ retrace [0-9]+ gx [0-9]+ dvd [0-9]+ [0-9]+" line "${output}")
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
list(GET lines 1 second)
if(NOT first STREQUAL second)
    message(FATAL_ERROR "ac_port_play: the runs differ:\n${first}\n${second}")
endif()
