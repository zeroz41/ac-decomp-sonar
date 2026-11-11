# The town replay's state hashes measured at a display's rate (port/NOTES.md, State hashes; sonarlib
# docs/roadmap.md, M6 step 9f3b):
#   cmake -DAC_IMPORT=<ac-import> -DAC_PORT=<ac_port> -DSL_REPLAY=<sl-replay> -DTOWN_SCRIPT=<town_replay.txt>
#         -DAC_DISC=<disc> -DAC_VERSION=<version> -DOUT_DIR=<dir> -DAC_LAVAPIPE_ICD=<lavapipe's ICD>
#         -P state_measured_test.cmake
# ac-import writes the package from the player's disc into OUT_DIR, and sl-replay builds the script into a recording
# there. The measured runs present on the GPU sink with no window, through SDL's offscreen video driver and lavapipe's
# Vulkan, with SDL's dummy audio driver: nothing opens on the desktop and nothing plays.
#
# The town replay runs headless with --state-hashes: it must exit 0 with the play line and the replay line at tick
# 38001 in the town, and write a line for each tick boundary, its ticks consecutive from 0. Then --measure --replay
# --ticks 3000 --state-hashes runs at --present-hz 144 and at 360, once each. Each must exit 0 with the measure line,
# a presents line at its rate with nothing backward or unexplained, and the measured line, and write its state hashes,
# ticks consecutive from 0, a prefix of the headless run's file. The two rates' measured lines must be equal.
#
# It prints every line it checks. When AC_DISC names no disc or there is no lavapipe it prints the line the test's
# SKIP_REGULAR_EXPRESSION matches and returns.
cmake_minimum_required(VERSION 3.25)

if("${AC_DISC}" STREQUAL "")
    message("ac_port_state_measured: skipped, AC_DISC is not set")
    return()
endif()
if("${AC_LAVAPIPE_ICD}" STREQUAL "")
    message("ac_port_state_measured: skipped, no lavapipe ICD for the offscreen driver's Vulkan")
    return()
endif()
foreach(variable IN ITEMS AC_IMPORT AC_PORT SL_REPLAY TOWN_SCRIPT AC_VERSION OUT_DIR)
    if("${${variable}}" STREQUAL "")
        message(FATAL_ERROR "ac_port_state_measured: ${variable} is not set")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUT_DIR}")
set(package "${OUT_DIR}/${AC_VERSION}.slpack")
execute_process(COMMAND "${AC_IMPORT}" "${AC_DISC}" "${package}" "${OUT_DIR}/cache"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ac_port_state_measured: ac-import failed (${result}):\n${output}${errors}")
endif()
message("ac-import: ${output}")

set(town "${OUT_DIR}/town_replay.slir")
file(REMOVE "${town}")
execute_process(COMMAND "${SL_REPLAY}" build "${TOWN_SCRIPT}" "${town}"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0 OR NOT EXISTS "${town}")
    message(FATAL_ERROR "ac_port_state_measured: sl-replay failed (${result}):\n${output}${errors}")
endif()
message("${output}")

set(count "[0-9]+")
set(line_pattern "ac_port: [a-z]+ tick ${count} retrace ${count} gx ${count} dvd ${count} ${count} hash [0-9a-f]+")
string(APPEND line_pattern " instances ${count} compiles ${count} holes ${count}/${count} cpu ${count}")
string(APPEND line_pattern " refused ${count} views ${count} invalid ${count} problems ${count} overflows ${count}")
string(APPEND line_pattern " unowned ${count} strays ${count} gx-errors ${count} unmapped ${count} outside ${count}")
string(APPEND line_pattern " nested ${count} refused-frames ${count} audio ${count} loud ${count} dsp ${count}")
string(APPEND line_pattern " sound [0-9a-f]+ scene ${count}")
set(time "${count} ${count} ${count}")
set(measure_pattern "ac_port: measure kit frames ${count} ticks ${count} game ${time}")
string(APPEND measure_pattern " prepare ${time} present ${time} gpu ${time} draws ${count} calls ${count}")
string(APPEND measure_pattern " pipelines ${count} buffers ${count} textures ${count} storage ${count} pushes ${count}")
set(presents_pattern "ac_port: presents ${count} count ${count} held ${count} late ${count} dropped ${count}")
string(APPEND presents_pattern " backward ${count} pairs ${count} unexplained ${count} none ${count}")
string(APPEND presents_pattern " unmatched ${count} explicit ${count} forget ${count} cut ${count} all ${count}")
string(APPEND presents_pattern " duplicate ${count} rotation ${count} distance ${count} gap ${count} auto-cut ${count}")

# Runs ac_port with the arguments given, on the offscreen driver, setting result, output, errors, lines, the report
# lines in order, and seconds, the run's time.
macro(run_port)
    string(TIMESTAMP started "%s")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy
                            "VK_DRIVER_FILES=${AC_LAVAPIPE_ICD}" "${AC_PORT}" --package "${package}" ${ARGN}
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 1200)
    string(TIMESTAMP ended "%s")
    math(EXPR seconds "${ended} - ${started}")
    string(REGEX MATCHALL "${line_pattern}" lines "${output}")
endmacro()

# The last 4000 bytes of a run's stderr, for a failure's message.
function(tail_of text out)
    string(LENGTH "${text}" length)
    if(length GREATER 4000)
        math(EXPR from "${length} - 4000")
        string(SUBSTRING "${text}" ${from} -1 text)
    endif()
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# Fails unless the state hashes file holds a line for each tick boundary, the tick and the hash as 16 hex digits, its
# ticks consecutive from 0, and sets out to its contents.
function(read_hashes name file out)
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "ac_port_state_measured: ${name} wrote no state hashes")
    endif()
    file(STRINGS "${file}" hash_lines)
    set(expected 0)
    foreach(hash_line IN LISTS hash_lines)
        set(tick "")
        if(hash_line MATCHES "^([0-9]+) [0-9a-f]+$")
            set(tick "${CMAKE_MATCH_1}")
        endif()
        if(NOT tick STREQUAL expected)
            message(FATAL_ERROR "ac_port_state_measured: ${name}'s state hashes have no line for tick ${expected}:\n"
                                "${hash_line}")
        endif()
        string(LENGTH "${hash_line}" length)
        string(LENGTH "${tick}" tick_length)
        math(EXPR hash_length "${length} - ${tick_length} - 1")
        if(NOT hash_length EQUAL 16)
            message(FATAL_ERROR "ac_port_state_measured: ${name}'s hash at tick ${expected} is not 16 hex digits:\n"
                                "${hash_line}")
        endif()
        math(EXPR expected "${expected} + 1")
    endforeach()
    if(expected EQUAL 0)
        message(FATAL_ERROR "ac_port_state_measured: ${name}'s state hashes are empty")
    endif()
    message("${name}: ${expected} state hashes, ticks 0 to ${tick}")
    file(READ "${file}" text)
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# The town replay headless, to its end.
set(hashes "${OUT_DIR}/hashes_headless.txt")
file(REMOVE "${hashes}")
run_port(--replay "${town}" --state-hashes "${hashes}")
message("town headless: exit ${result}, ${seconds} s:\n${lines}")
list(LENGTH lines found)
set(replay_line "")
if(found EQUAL 2)
    list(GET lines 1 replay_line)
endif()
if(NOT result EQUAL 0 OR NOT replay_line MATCHES "^ac_port: replay tick 38001 .* scene 7$")
    tail_of("${errors}" errors)
    message(FATAL_ERROR "ac_port_state_measured: the headless run did not end with the replay in the town at tick "
                        "38001:\n${errors}")
endif()
read_hashes("the headless run" "${hashes}" headless_hashes)

# The town replay's first 3000 ticks measured at each rate.
set(measured_lines "")
foreach(hz IN ITEMS 144 360)
    set(hashes "${OUT_DIR}/hashes_${hz}.txt")
    file(REMOVE "${hashes}")
    run_port(--measure --replay "${town}" --ticks 3000 --present-hz ${hz} --state-hashes "${hashes}")
    string(REGEX MATCHALL "${measure_pattern}" measure_lines "${output}")
    string(REGEX MATCHALL "${presents_pattern}" presents_lines "${output}")
    message("town measured at ${hz} Hz: exit ${result}, ${seconds} s:\n${measure_lines}\n${presents_lines}\n${lines}")
    list(LENGTH measure_lines measures)
    list(LENGTH presents_lines presents)
    list(LENGTH lines found)
    set(measured_line "")
    if(found EQUAL 2)
        list(GET lines 1 measured_line)
    endif()
    if(NOT result EQUAL 0 OR NOT measures EQUAL 1 OR NOT presents EQUAL 1
       OR NOT measured_line MATCHES "^ac_port: measured ")
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_state_measured: the run at ${hz} Hz did not end with the measure, presents and "
                            "measured lines:\n${output}\n${errors}")
    endif()
    if(NOT presents_lines MATCHES "^ac_port: presents ${hz} " OR NOT presents_lines MATCHES " backward 0 "
       OR NOT presents_lines MATCHES " unexplained 0 ")
        message(FATAL_ERROR "ac_port_state_measured: the run at ${hz} Hz is not at that rate, or went backward or "
                            "snapped unexplained:\n${presents_lines}")
    endif()
    read_hashes("the run at ${hz} Hz" "${hashes}" rate_hashes)
    string(LENGTH "${rate_hashes}" length)
    string(SUBSTRING "${headless_hashes}" 0 ${length} start)
    if(NOT start STREQUAL rate_hashes)
        message(FATAL_ERROR "ac_port_state_measured: the state hashes of the run at ${hz} Hz do not begin the "
                            "headless run's")
    endif()
    list(APPEND measured_lines "${measured_line}")
endforeach()
list(GET measured_lines 0 at_144)
list(GET measured_lines 1 at_360)
if(NOT at_144 STREQUAL at_360)
    message(FATAL_ERROR "ac_port_state_measured: the measured lines at 144 Hz and 360 Hz differ:\n"
                        "${at_144}\n${at_360}")
endif()
