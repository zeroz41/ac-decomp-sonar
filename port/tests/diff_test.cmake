# Runs ac_port_reference --diff from boot to play's first frame, then over the town replay (sonarlib
# docs/roadmap.md, M6 steps 6j, 9b, 10a and 10b):
#   cmake -DAC_IMPORT=<ac-import> -DAC_PORT_REFERENCE=<ac_port_reference> -DSL_REPLAY=<sl-replay>
#         -DTOWN_SCRIPT=<town_replay.txt> -DAC_DISC=<disc> -DAC_VERSION=<version> -DOUT_DIR=<dir> -P diff_test.cmake
# ac-import writes the package from the player's disc into OUT_DIR, then each run draws every emu64 task through the
# kit and through the console's emu64, the reference, each into a frame of its own, and compares the two captures and
# their counts of copies. The first must reach play's first frame and exit 0 with tasks compared, none different, none
# refused and no copies, and a play line with its unowned and strays. The second runs --diff-from 1000 --replay on the
# town replay cut at tick 8800, which sl-replay builds into OUT_DIR: past the title demo, and through the name
# keyboard's copy at 8757. It must exit 0 with the replay line at 8801, its gx-errors and refused 0, and a last diff
# line with tasks compared, none different, none refused, and the keyboard's copy. Each prints its differences, its
# diff lines and its play or replay line. When AC_DISC names no disc it prints the line the test's
# SKIP_REGULAR_EXPRESSION matches and returns.
cmake_minimum_required(VERSION 3.25)

if("${AC_DISC}" STREQUAL "")
    message("ac_port_diff: skipped, AC_DISC is not set")
    return()
endif()
foreach(variable IN ITEMS AC_IMPORT AC_PORT_REFERENCE SL_REPLAY TOWN_SCRIPT AC_VERSION OUT_DIR)
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
string(APPEND diff_pattern " triangles ${count} skipped ${count} ${count} copies ${count}")
string(TIMESTAMP started "%s")
execute_process(COMMAND "${AC_PORT_REFERENCE}" --package "${package}" --diff
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 1200)
string(TIMESTAMP ended "%s")
math(EXPR seconds "${ended} - ${started}")
string(REGEX MATCHALL "ac_port: diff tick [^\n]*" differences "${output}")
foreach(difference IN LISTS differences)
    message("${difference}")
endforeach()
string(REGEX MATCH "${diff_pattern}" line "${output}")
string(REGEX MATCH "ac_port: play [^\n]* unowned ${count} strays ${count} [^\n]*" play "${output}")
message("exit ${result}, ${seconds} s:\n${line}\n${play}")
if(NOT result EQUAL 0 OR line STREQUAL "" OR play STREQUAL "")
    string(LENGTH "${errors}" length)
    if(length GREATER 4000)
        math(EXPR from "${length} - 4000")
        string(SUBSTRING "${errors}" ${from} -1 errors)
    endif()
    message(FATAL_ERROR "ac_port_diff: the run did not reach play's first frame:\n${errors}")
endif()
# A diff line's tasks compared, none different and none refused, and its copies: copies.
function(check_diff line copies)
    string(REGEX MATCH "tasks ([0-9]+) equal [0-9]+ different ([0-9]+) refused ([0-9]+) .* copies ([0-9]+)$" ignored
           "${line}")
    if(CMAKE_MATCH_1 EQUAL 0)
        message(FATAL_ERROR "ac_port_diff: no task was compared:\n${line}")
    endif()
    if(NOT CMAKE_MATCH_2 EQUAL 0 OR NOT CMAKE_MATCH_3 EQUAL 0)
        message(FATAL_ERROR "ac_port_diff: the kit and the reference differ, or a task was refused:\n${line}")
    endif()
    if(NOT CMAKE_MATCH_4 EQUAL copies)
        message(FATAL_ERROR "ac_port_diff: the kit's copies are not ${copies}:\n${line}")
    endif()
endfunction()
check_diff("${line}" 0)

# The town replay to tick 8800: its ticks line and its events before 8800. The lines it keeps hold no semicolon.
file(STRINGS "${TOWN_SCRIPT}" events REGEX "^([0-9]+ |ticks )")
set(script "")
foreach(event IN LISTS events)
    if(event MATCHES "^ticks ")
        string(APPEND script "ticks 8800\n")
    elseif(event MATCHES "^([0-9]+) " AND CMAKE_MATCH_1 LESS 8800)
        string(APPEND script "${event}\n")
    endif()
endforeach()
file(WRITE "${OUT_DIR}/town_8800.txt" "${script}")
set(recording "${OUT_DIR}/town_8800.slir")
file(REMOVE "${recording}")
execute_process(COMMAND "${SL_REPLAY}" build "${OUT_DIR}/town_8800.txt" "${recording}"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0 OR NOT EXISTS "${recording}")
    message(FATAL_ERROR "ac_port_diff: sl-replay failed (${result}):\n${output}${errors}")
endif()

string(TIMESTAMP started "%s")
execute_process(COMMAND "${AC_PORT_REFERENCE}" --package "${package}" --diff --diff-from 1000 --replay "${recording}"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 2400)
string(TIMESTAMP ended "%s")
math(EXPR seconds "${ended} - ${started}")
string(REGEX MATCHALL "ac_port: diff tick [^\n]*" differences "${output}")
foreach(difference IN LISTS differences)
    message("${difference}")
endforeach()
string(REGEX MATCHALL "${diff_pattern}" lines "${output}")
string(REGEX MATCH "ac_port: replay tick 8801 [^\n]* gx-errors ${count} [^\n]*" replay "${output}")
message("replay: exit ${result}, ${seconds} s:")
foreach(line IN LISTS lines)
    message("${line}")
endforeach()
message("${replay}")
if(NOT result EQUAL 0 OR lines STREQUAL "" OR replay STREQUAL "")
    string(LENGTH "${errors}" length)
    if(length GREATER 4000)
        math(EXPR from "${length} - 4000")
        string(SUBSTRING "${errors}" ${from} -1 errors)
    endif()
    message(FATAL_ERROR "ac_port_diff: the replay did not end at tick 8801:\n${errors}")
endif()
list(GET lines -1 line)
check_diff("${line}" 1)
string(REGEX MATCH " refused ([0-9]+) views .* gx-errors ([0-9]+) " ignored "${replay}")
if(NOT CMAKE_MATCH_1 EQUAL 0 OR NOT CMAKE_MATCH_2 EQUAL 0)
    message(FATAL_ERROR "ac_port_diff: the replay's frames refused draws, or its GX glue failed:\n${replay}")
endif()
