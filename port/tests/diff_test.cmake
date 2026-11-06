# Runs ac_port --diff from boot to play's first frame (sonarlib docs/roadmap.md, M6 step 6j):
#   cmake -DAC_IMPORT=<ac-import> -DAC_PORT=<ac_port> -DAC_DISC=<disc> -DAC_VERSION=<version> -DOUT_DIR=<dir>
#         -P diff_test.cmake
# ac-import writes the package from the player's disc into OUT_DIR, then the run draws every emu64 task through the
# kit and through the console's emu64, the reference, each into a frame of its own, and compares the two captures. It
# must reach play's first frame and exit 0 with tasks compared, none different and none refused. It prints the
# differences, the diff line and the play line. When AC_DISC names no disc it prints the line the test's
# SKIP_REGULAR_EXPRESSION matches and returns.
cmake_minimum_required(VERSION 3.25)

if("${AC_DISC}" STREQUAL "")
    message("ac_port_diff: skipped, AC_DISC is not set")
    return()
endif()
foreach(variable IN ITEMS AC_IMPORT AC_PORT AC_VERSION OUT_DIR)
    if("${${variable}}" STREQUAL "")
        message(FATAL_ERROR "ac_port_diff: ${variable} is not set")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUT_DIR}")
set(package "${OUT_DIR}/${AC_VERSION}.slpack")
execute_process(COMMAND "${AC_IMPORT}" "${AC_DISC}" "${package}" "${OUT_DIR}/cache"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ac_port_diff: ac-import failed (${result}):\n${output}${errors}")
endif()
message("ac-import: ${output}")

set(count "[0-9]+")
set(diff_pattern "ac_port: diff tasks ${count} equal ${count} different ${count} refused ${count}")
string(APPEND diff_pattern " triangles ${count} skipped ${count} ${count}")
string(TIMESTAMP started "%s")
execute_process(COMMAND "${AC_PORT}" --package "${package}" --diff
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 1200)
string(TIMESTAMP ended "%s")
math(EXPR seconds "${ended} - ${started}")
string(REGEX MATCHALL "ac_port: diff tick [^\n]*" differences "${output}")
foreach(difference IN LISTS differences)
    message("${difference}")
endforeach()
string(REGEX MATCH "${diff_pattern}" line "${output}")
string(REGEX MATCH "ac_port: play [^\n]*" play "${output}")
message("exit ${result}, ${seconds} s:\n${line}\n${play}")
if(NOT result EQUAL 0 OR line STREQUAL "" OR play STREQUAL "")
    string(LENGTH "${errors}" length)
    if(length GREATER 4000)
        math(EXPR from "${length} - 4000")
        string(SUBSTRING "${errors}" ${from} -1 errors)
    endif()
    message(FATAL_ERROR "ac_port_diff: the run did not reach play's first frame:\n${errors}")
endif()
string(REGEX MATCH "tasks ([0-9]+) equal [0-9]+ different ([0-9]+) refused ([0-9]+) " ignored "${line}")
if(CMAKE_MATCH_1 EQUAL 0)
    message(FATAL_ERROR "ac_port_diff: no task was compared:\n${line}")
endif()
if(NOT CMAKE_MATCH_2 EQUAL 0 OR NOT CMAKE_MATCH_3 EQUAL 0)
    message(FATAL_ERROR "ac_port_diff: the kit and the reference differ, or a task was refused:\n${line}")
endif()
