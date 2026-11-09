# Records the title's input in a window from a device script and replays it (port/NOTES.md, Recording; sonarlib
# docs/roadmap.md, M6 step 9f2b):
#   cmake -DAC_IMPORT=<ac-import> -DAC_PORT=<ac_port> -DSL_REPLAY=<sl-replay> -DSCRIPT=<title_input.txt>
#         -DSET_SCRIPT=<title_input_set.txt> -DAC_DISC=<disc> -DAC_VERSION=<version> -DOUT_DIR=<dir>
#         -DAC_LAVAPIPE_ICD=<lavapipe's ICD> -P record_test.cmake
# ac-import writes the package from the player's disc into OUT_DIR. Each window run shows the game on SDL's offscreen
# video driver, through lavapipe's Vulkan, with SDL's dummy audio driver, the record runs keeping their settings under
# OUT_DIR/user and the window replays theirs under OUT_DIR/replay_user: nothing opens on the desktop, nothing plays, no
# host device takes a port and no player's file is read or written.
#
# SCRIPT is recorded under --window --ticks 1200, paced. The run must exit 0, take no less than 20 s, and print the
# play line, the record line, the saves line and the window line, with card-a, which the run creates, in slot A.
# sl-replay show must give the gamepad's connection at tick 0, Start down at 1000 and up at 1006, the calendar, the
# package, os's settings, the port's data and no saves. The recording replays headless, then with --window --unpaced,
# and each must print the record run's play line, its record line, named replay, and its saves line.
#
# SET_SCRIPT is recorded unpaced with --set sonar.system.progressive_scan=true. show must give progressive scan in its
# settings, the clock offset changed at tick 300 and the first run's card, and its replays its record run's lines,
# which differ from the first's. The record run must not store the --set in the settings file, nor the window replays
# any setting.
#
# Usage errors: --record without --window, --input-script without --record, a window replay with --set naming one of
# os's settings, which the recording gives, and --qrand-seed with a recording that carries the port's data, must each
# exit 2 and write no recording.
#
# It prints every line it checks. When AC_DISC names no disc or there is no lavapipe it prints the line the test's
# SKIP_REGULAR_EXPRESSION matches and returns.
cmake_minimum_required(VERSION 3.25)

if("${AC_DISC}" STREQUAL "")
    message("ac_port_record: skipped, AC_DISC is not set")
    return()
endif()
if("${AC_LAVAPIPE_ICD}" STREQUAL "")
    message("ac_port_record: skipped, no lavapipe ICD for the offscreen driver's Vulkan surface")
    return()
endif()
foreach(variable IN ITEMS AC_IMPORT AC_PORT SL_REPLAY SCRIPT SET_SCRIPT AC_VERSION OUT_DIR)
    if("${${variable}}" STREQUAL "")
        message(FATAL_ERROR "ac_port_record: ${variable} is not set")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUT_DIR}")
set(package "${OUT_DIR}/${AC_VERSION}.slpack")
execute_process(COMMAND "${AC_IMPORT}" "${AC_DISC}" "${package}" "${OUT_DIR}/cache"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ac_port_record: ac-import failed (${result}):\n${output}${errors}")
endif()
message("ac-import: ${output}")

set(user "${OUT_DIR}/user")
set(replay_user "${OUT_DIR}/replay_user")
set(first "${OUT_DIR}/title_input.slir")
set(second "${OUT_DIR}/title_input_set.slir")
set(refused "${OUT_DIR}/refused.slir")
file(REMOVE_RECURSE "${user}" "${replay_user}")
file(REMOVE "${first}" "${second}" "${refused}")

set(count "[0-9]+")
set(line_pattern "ac_port: [a-z]+ tick ${count} retrace ${count} gx ${count} dvd ${count} ${count} hash [0-9a-f]+")
string(APPEND line_pattern " instances ${count} compiles ${count} holes ${count}/${count} cpu ${count}")
string(APPEND line_pattern " refused ${count} views ${count} invalid ${count} problems ${count} overflows ${count}")
string(APPEND line_pattern " unowned ${count} strays ${count} gx-errors ${count} unmapped ${count} outside ${count}")
string(APPEND line_pattern " nested ${count} refused-frames ${count} audio ${count} loud ${count} dsp ${count}")
string(APPEND line_pattern " sound [0-9a-f]+ scene ${count}")

# Runs ac_port with the arguments given, headless unless they hold --window, which runs offscreen, setting result,
# output, errors, lines, the play and record or replay lines in order, saves_line, window_line, and seconds, the run's
# time.
macro(run_port)
    string(TIMESTAMP started "%s")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy
                            "VK_DRIVER_FILES=${AC_LAVAPIPE_ICD}" "${AC_PORT}" --package "${package}" ${ARGN}
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 600)
    string(TIMESTAMP ended "%s")
    math(EXPR seconds "${ended} - ${started}")
    string(REGEX MATCHALL "${line_pattern}" lines "${output}")
    string(REGEX MATCH "ac_port: saves [^\n]*" saves_line "${output}")
    string(REGEX MATCH "ac_port: window [^\n]*" window_line "${output}")
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

# Records `script` into `recording` with the further arguments given, and checks the run's lines, its saves line and
# its window line, setting `name`_lines to its play and record lines and `name`_saves to its saves line.
macro(record name recording script)
    run_port(--window --user-dir "${user}" --ticks 1200 --record "${recording}" --input-script "${script}" ${ARGN})
    message("${name} record: exit ${result}, ${seconds} s:\n${lines}\n${saves_line}\n${window_line}")
    list(LENGTH lines found)
    if(NOT result EQUAL 0 OR NOT found EQUAL 2 OR saves_line STREQUAL "" OR window_line STREQUAL ""
       OR NOT EXISTS "${recording}")
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_record: the ${name} record run did not print its lines, the saves line and the "
                            "window line:\n${errors}")
    endif()
    list(GET lines 0 play_line)
    list(GET lines 1 record_line)
    if(NOT play_line MATCHES "^ac_port: play " OR NOT record_line MATCHES "^ac_port: record tick 1201 ")
        message(FATAL_ERROR "ac_port_record: the ${name} record run's lines are not a play and a record line")
    endif()
    string(FIND "${output}" "${record_line}" record_at)
    string(FIND "${output}" "${saves_line}" saves_at)
    string(FIND "${output}" "${window_line}" window_at)
    if(saves_at LESS record_at OR window_at LESS saves_at OR NOT window_line MATCHES " cards card-a -$")
        message(FATAL_ERROR "ac_port_record: the ${name} record run's saves line and window line do not follow the "
                            "record line, with card-a in slot A:\n${output}")
    endif()
    set(${name}_lines "${lines}")
    set(${name}_saves "${saves_line}")
endmacro()

# Checks that sl-replay show of `recording` holds each of the further arguments, as a whole line.
function(check_show name recording)
    execute_process(COMMAND "${SL_REPLAY}" show "${recording}" RESULT_VARIABLE result OUTPUT_VARIABLE shown
                    ERROR_VARIABLE errors)
    message("${name} show: exit ${result}:\n${shown}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "ac_port_record: sl-replay show failed on the ${name} recording:\n${errors}")
    endif()
    foreach(expected IN LISTS ARGN)
        if(NOT shown MATCHES "(^|\n)${expected}\n")
            message(FATAL_ERROR "ac_port_record: the ${name} recording's show has no line ${expected}")
        endif()
    endforeach()
endfunction()

# Replays `recording` headless, then in a window, unpaced, and checks that each prints the lines in `name`_lines,
# the record line named replay, and the saves line in `name`_saves.
function(check_replays name recording)
    set(expected "${${name}_lines}")
    string(REPLACE "ac_port: record " "ac_port: replay " expected "${expected}")
    run_port(--replay "${recording}")
    message("${name} headless replay: exit ${result}, ${seconds} s:\n${lines}\n${saves_line}")
    if(NOT result EQUAL 0 OR NOT lines STREQUAL expected OR NOT saves_line STREQUAL "${${name}_saves}")
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_record: the ${name} recording's headless replay did not print the record run's "
                            "lines and its saves line:\n${lines}\n${saves_line}\n${expected}\n${errors}")
    endif()
    run_port(--window --user-dir "${replay_user}" --unpaced --replay "${recording}")
    message("${name} window replay: exit ${result}, ${seconds} s:\n${lines}\n${saves_line}\n${window_line}")
    if(NOT result EQUAL 0 OR window_line STREQUAL "" OR NOT lines STREQUAL expected
       OR NOT saves_line STREQUAL "${${name}_saves}")
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_record: the ${name} recording's window replay did not print the record run's "
                            "lines, its saves line and the window line:\n${lines}\n${saves_line}\n${expected}\n"
                            "${errors}")
    endif()
endfunction()

# The settings file under `directory`, or empty when there is none.
function(read_settings directory out)
    set(text "")
    if(EXISTS "${directory}/config/settings.toml")
        file(READ "${directory}/config/settings.toml" text)
    endif()
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

set(calendar_line "calendar year ${count} month ${count} day ${count} hour ${count} minute ${count} second ${count}")
string(APPEND calendar_line " millisecond ${count} microsecond ${count} weekday ${count} year-day ${count}")
set(common_lines "version 2" "ticks 1200 retraces 1200 events 3" "packages 1"
                 "package game importer ac ${count} hash [0-9a-f]+" "${calendar_line}" "port-data 6 0100[0-9a-f]+"
                 "event 0 tick 0 retrace 0 port 0 connected gamecube" "event 1 tick 1000 retrace 1000 port 0 start down"
                 "event 2 tick 1006 retrace 1006 port 0 start up")

# The title's input, paced.
record(first "${first}" "${SCRIPT}")
if(seconds LESS 20)
    message(FATAL_ERROR "ac_port_record: the paced record run took ${seconds} s, less than its 20 s")
endif()
check_show(first "${first}" ${common_lines} "set sonar.system.progressive_scan=false" "saves 0")
check_replays(first "${first}")

# The same with progressive scan on from the start and the clock offset set at tick 300, unpaced.
record(second "${second}" "${SET_SCRIPT}" --unpaced --set sonar.system.progressive_scan=true)
check_show(second "${second}" ${common_lines} "set sonar.system.progressive_scan=true"
           "settings tick 300 sonar.system.clock_offset=1749600000000" "saves 1"
           "save card-a ${count} hash [0-9a-f]+")
check_replays(second "${second}")
list(GET first_lines 1 first_record)
list(GET second_lines 1 second_record)
if(first_record STREQUAL second_record)
    message(FATAL_ERROR "ac_port_record: the two recordings' record lines are the same:\n${first_record}")
endif()

# The --set lasted for its run, and the window replays left the player's settings alone.
read_settings("${user}" record_settings)
read_settings("${replay_user}" replay_settings)
message("the record runs' settings:\n${record_settings}\nthe window replays' settings:\n${replay_settings}")
if(record_settings MATCHES "progressive_scan")
    message(FATAL_ERROR "ac_port_record: the record run stored its --set in the settings file")
endif()
if(replay_settings MATCHES "progressive_scan|clock_offset")
    message(FATAL_ERROR "ac_port_record: a window replay stored the recording's settings in the settings file")
endif()

# Usage errors.
set(progressive "sonar.system.progressive_scan=true")
foreach(arguments IN ITEMS "--record;${refused};--ticks;1200"
                           "--window;--user-dir;${user};--ticks;10;--input-script;${SCRIPT}"
                           "--window;--user-dir;${replay_user};--replay;${first};--set;${progressive}"
                           "--replay;${first};--qrand-seed;1234")
    run_port(${arguments})
    message("${arguments}: exit ${result}")
    if(NOT result EQUAL 2 OR EXISTS "${refused}")
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_record: ${arguments} is not a usage error:\n${errors}")
    endif()
endforeach()
