# Saves a new town in a window from a device script, loads it after a restart, and replays both (port/NOTES.md, Saves in
# recordings; sonarlib docs/roadmap.md, M6 step 9f2c):
#   cmake -DAC_IMPORT=<ac-import> -DAC_PORT=<ac_port> -DSL_REPLAY=<sl-replay> -DSL_CARD=<sl-card>
#         -DSAVE_SCRIPT=<save_input.txt> -DLOAD_SCRIPT=<load_input.txt> -DAC_DISC=<disc> -DAC_VERSION=<version>
#         -DOUT_DIR=<dir> -DAC_LAVAPIPE_ICD=<lavapipe's ICD> -P save_test.cmake
# ac-import writes the package from the player's disc into OUT_DIR. Each window run shows the game on SDL's offscreen
# video driver, through lavapipe's Vulkan, with SDL's dummy audio driver, the record runs keeping their cards under
# OUT_DIR/user and the window replay its settings under OUT_DIR/replay_user: nothing opens on the desktop, nothing
# plays, no host device takes a port and no player's file is read or written. Each takes --set
# sonar.video.render_scale=1, so lavapipe draws the game's own 640x480 and not the 1280x960 the window's size asks for
# by default (roadmap.md, M6 step 9c1).
#
# SAVE_SCRIPT is recorded under --window --unpaced --date 2001-12-14T12:00:00 on an empty user directory, and its close
# ends the run. The run must exit 0 and print the play line, the record line, the saves line with 2 saves, and the
# window line with card-a in slot A and the target of render scale 1, 640x480, and sl-replay show must give saves 0.
# sl-card extract of the town file must give zeros at its data's 0xDE0 to 0xE40 and 0x1320 to 0x1440, past the winter
# banner's 208 entries and the icon's 112.
#
# LOAD_SCRIPT is recorded the same way on the same directory. Its record line must end at scene 7, the town, and its
# saves line differ from the save's, and show must give saves 2, with card-a and card-a-f000.
#
# Each recording replays headless to its record run's lines, the record line named replay, and the load's again with
# --window --unpaced on a user directory of its own, which has no saves afterward. The record runs' save directory
# must be byte for byte the same before the replays and after.
#
# Usage errors: --date without --record, and --date 1999-12-31T00:00:00, must each exit 2 and write no recording.
#
# It prints every line it checks. When AC_DISC names no disc or there is no lavapipe it prints the line the test's
# SKIP_REGULAR_EXPRESSION matches and returns.
cmake_minimum_required(VERSION 3.25)

if("${AC_DISC}" STREQUAL "")
    message("ac_port_save: skipped, AC_DISC is not set")
    return()
endif()
if("${AC_LAVAPIPE_ICD}" STREQUAL "")
    message("ac_port_save: skipped, no lavapipe ICD for the offscreen driver's Vulkan surface")
    return()
endif()
foreach(variable IN ITEMS AC_IMPORT AC_PORT SL_REPLAY SL_CARD SAVE_SCRIPT LOAD_SCRIPT AC_VERSION OUT_DIR)
    if("${${variable}}" STREQUAL "")
        message(FATAL_ERROR "ac_port_save: ${variable} is not set")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUT_DIR}")
set(package "${OUT_DIR}/${AC_VERSION}.slpack")
execute_process(COMMAND "${AC_IMPORT}" "${AC_DISC}" "${package}" "${OUT_DIR}/cache"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ac_port_save: ac-import failed (${result}):\n${output}${errors}")
endif()
message("ac-import: ${output}")

set(user "${OUT_DIR}/user")
set(replay_user "${OUT_DIR}/replay_user")
set(extracted "${OUT_DIR}/extracted")
set(save "${OUT_DIR}/save_input.slir")
set(load "${OUT_DIR}/load_input.slir")
set(refused "${OUT_DIR}/refused.slir")
file(REMOVE_RECURSE "${user}" "${replay_user}" "${extracted}")
file(REMOVE "${save}" "${load}" "${refused}")
set(date "2001-12-14T12:00:00")

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
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 1200)
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

# Records `script` into `recording` until its close, and checks the run's lines, its saves line, with 2 saves, and its
# window line, with card-a in slot A, setting `name`_lines to its play and record lines and `name`_saves to its saves
# line.
macro(record name recording script)
    run_port(--window --user-dir "${user}" --unpaced --date "${date}" --record "${recording}" --input-script "${script}"
             --set sonar.video.render_scale=1)
    message("${name} record: exit ${result}, ${seconds} s:\n${lines}\n${saves_line}\n${window_line}")
    list(LENGTH lines found)
    if(NOT result EQUAL 0 OR NOT found EQUAL 2 OR window_line STREQUAL "" OR NOT EXISTS "${recording}")
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_save: the ${name} record run did not print its lines and the window line:\n"
                            "${errors}")
    endif()
    list(GET lines 0 play_line)
    list(GET lines 1 record_line)
    if(NOT play_line MATCHES "^ac_port: play " OR NOT record_line MATCHES "^ac_port: record ")
        message(FATAL_ERROR "ac_port_save: the ${name} record run's lines are not a play and a record line")
    endif()
    if(NOT saves_line MATCHES "^ac_port: saves 2 hash [0-9a-f]+$")
        message(FATAL_ERROR "ac_port_save: the ${name} record run's saves line does not give 2 saves:\n${output}")
    endif()
    string(FIND "${output}" "${record_line}" record_at)
    string(FIND "${output}" "${saves_line}" saves_at)
    string(FIND "${output}" "${window_line}" window_at)
    if(saves_at LESS record_at OR window_at LESS saves_at OR NOT window_line MATCHES
       " cards card-a - draw-scale 16 draw-scale-changes 0 gpu-timed 0 gpu-over 0 target 640x480$")
        message(FATAL_ERROR "ac_port_save: the ${name} record run's saves line and window line do not follow the "
                            "record line, with card-a in slot A and the target at scale 1, drawn at it and "
                            "untimed:\n${output}")
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
        message(FATAL_ERROR "ac_port_save: sl-replay show failed on the ${name} recording:\n${errors}")
    endif()
    foreach(expected IN LISTS ARGN)
        if(NOT shown MATCHES "(^|\n)${expected}\n")
            message(FATAL_ERROR "ac_port_save: the ${name} recording's show has no line ${expected}")
        endif()
    endforeach()
endfunction()

# Checks that the run just ended printed the lines in `name`_lines, the record line named replay, and the saves line in
# `name`_saves, naming the run `what` in a failure's message.
function(check_replayed name what)
    set(expected "${${name}_lines}")
    string(REPLACE "ac_port: record " "ac_port: replay " expected "${expected}")
    if(NOT result EQUAL 0 OR NOT lines STREQUAL expected OR NOT saves_line STREQUAL "${${name}_saves}")
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_save: the ${name} recording's ${what} did not print the record run's lines and "
                            "its saves line:\n${lines}\n${saves_line}\n${expected}\n${${name}_saves}\n${errors}")
    endif()
endfunction()

# Each file under `directory`, with its SHA-256, or empty when there is none.
function(hash_files directory out)
    set(hashes "")
    file(GLOB_RECURSE files LIST_DIRECTORIES false "${directory}/*")
    list(SORT files)
    foreach(path IN LISTS files)
        file(SHA256 "${path}" hash)
        string(APPEND hashes "${path} ${hash}\n")
    endforeach()
    set(${out} "${hashes}" PARENT_SCOPE)
endfunction()

# Checks that the town file's data, after the GCI's 0x40-byte entry, is zeros for `size` bytes from `offset`.
function(check_zeros town offset size)
    math(EXPR at "0x40 + ${offset}")
    math(EXPR limit "${size}")
    file(READ "${town}" bytes OFFSET ${at} LIMIT ${limit} HEX)
    string(LENGTH "${bytes}" length)
    math(EXPR expected "${size} * 2")
    math(EXPR end "${offset} + ${size}" OUTPUT_FORMAT HEXADECIMAL)
    message("the town file's data from ${offset} to ${end}: ${bytes}")
    if(NOT length EQUAL expected OR NOT bytes MATCHES "^0+$")
        message(FATAL_ERROR "ac_port_save: the town file's data from ${offset} to ${end} is not zeros")
    endif()
endfunction()

# The save: a new town saved to card-a, which the record run creates.
record(save "${save}" "${SAVE_SCRIPT}")
check_show(save "${save}" "saves 0")

# The faults: the town file holds no bytes past the winter banner's palette and the icon's.
execute_process(COMMAND "${SL_CARD}" --user-dir "${user}" extract card-a "${extracted}" --replace
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
message("sl-card extract: exit ${result}:\n${output}")
file(GLOB towns "${extracted}/*-GAFE-DobutsunomoriP_MURA.gci")
list(LENGTH towns found)
if(NOT result EQUAL 0 OR NOT found EQUAL 1)
    message(FATAL_ERROR "ac_port_save: sl-card extract gave no town file:\n${output}${errors}")
endif()
check_zeros("${towns}" 0xDE0 0x60)
check_zeros("${towns}" 0x1320 0x120)

# The load: the town, after a restart, from the cards the save left.
record(load "${load}" "${LOAD_SCRIPT}")
list(GET load_lines 1 load_record)
if(NOT load_record MATCHES " scene 7$")
    message(FATAL_ERROR "ac_port_save: the load record run did not end in the town, scene 7:\n${load_record}")
endif()
if(load_saves STREQUAL save_saves)
    message(FATAL_ERROR "ac_port_save: the load's saves line is the save's:\n${load_saves}")
endif()
check_show(load "${load}" "saves 2" "save card-a ${count} hash [0-9a-f]+" "save card-a-f000 ${count} hash [0-9a-f]+")

# The replays, on memory stores seeded from the recordings, leave the player's cards alone.
hash_files("${user}/saves" before)
message("the record runs' saves:\n${before}")
run_port(--replay "${save}")
message("save headless replay: exit ${result}, ${seconds} s:\n${lines}\n${saves_line}")
check_replayed(save "headless replay")
run_port(--replay "${load}")
message("load headless replay: exit ${result}, ${seconds} s:\n${lines}\n${saves_line}")
check_replayed(load "headless replay")
run_port(--window --user-dir "${replay_user}" --unpaced --replay "${load}" --set sonar.video.render_scale=1)
message("load window replay: exit ${result}, ${seconds} s:\n${lines}\n${saves_line}\n${window_line}")
if(window_line STREQUAL "")
    message(FATAL_ERROR "ac_port_save: the load's window replay printed no window line")
endif()
check_replayed(load "window replay")
hash_files("${replay_user}/saves" replay_saves)
if(NOT replay_saves STREQUAL "")
    message(FATAL_ERROR "ac_port_save: the window replay wrote saves to its user directory:\n${replay_saves}")
endif()
hash_files("${user}/saves" after)
if(NOT after STREQUAL before)
    message(FATAL_ERROR "ac_port_save: the replays changed the record runs' saves:\n${after}")
endif()

# Usage errors.
foreach(arguments IN ITEMS "--window;--user-dir;${replay_user};--ticks;10;--date;${date}"
                           "--window;--user-dir;${replay_user};--ticks;10;--record;${refused};--date;1999-12-31T00:00:00")
    run_port(${arguments})
    message("${arguments}: exit ${result}")
    if(NOT result EQUAL 2 OR EXISTS "${refused}")
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_save: ${arguments} is not a usage error:\n${errors}")
    endif()
endforeach()
