# Runs the town and title replays headless and in a window (port/NOTES.md, Replays; sonarlib docs/roadmap.md, M6 steps
# 9f2a and 9f3a):
#   cmake -DAC_IMPORT=<ac-import> -DAC_PORT=<ac_port> -DSL_REPLAY=<sl-replay> -DTOWN_SCRIPT=<town_replay.txt>
#         -DTITLE_SCRIPT=<title_replay.txt> -DAC_DISC=<disc> -DAC_VERSION=<version> -DOUT_DIR=<dir>
#         -DAC_LAVAPIPE_ICD=<lavapipe's ICD> -P window_replay_test.cmake
# ac-import writes the package from the player's disc into OUT_DIR, and sl-replay builds the two scripts into
# recordings there. Each window run shows the game on SDL's offscreen video driver, through lavapipe's Vulkan, with
# SDL's dummy audio driver, and keeps its settings under OUT_DIR/user: nothing opens on the desktop, nothing plays and
# no player's file is read or written. Each takes --set sonar.video.render_scale=1, so lavapipe draws the game's own
# 640x480 and not the 1280x960 the window's size asks for by default (roadmap.md, M6 step 9c1).
#
# The town replay runs headless, then with --window --unpaced. The window run must exit 0, its play line and its one
# replay line must be the headless run's, byte for byte, and its window line must follow them, with no card in either
# slot and the target of render scale 1, 640x480.
#
# The title replay runs headless, then with --window, paced. The window run must exit 0, its replay line must be the
# headless run's, in scene 28, it must take no less than 20 s: 1200 ticks at 60000/1001 Hz, and its window line must
# count presents blended, as the window interpolates by default.
#
# Interpolation off: the paced title replay runs again with --set sonar.video.interpolation=false. It must exit 0 with
# the headless run's lines, and its window line must count no present blended and no more frames than ticks, as the
# window presents each frame once. The settings file afterward must hold no interpolation, as the --set lasted for its
# run.
#
# Usage errors: --unpaced without --replay, --unpaced without --window, and a window replay with --set naming one of
# os's settings, which a replay takes from its recording, must each exit 2.
#
# It prints every line it checks. When AC_DISC names no disc or there is no lavapipe it prints the line the test's
# SKIP_REGULAR_EXPRESSION matches and returns.
cmake_minimum_required(VERSION 3.25)

if("${AC_DISC}" STREQUAL "")
    message("ac_port_window_replay: skipped, AC_DISC is not set")
    return()
endif()
if("${AC_LAVAPIPE_ICD}" STREQUAL "")
    message("ac_port_window_replay: skipped, no lavapipe ICD for the offscreen driver's Vulkan surface")
    return()
endif()
foreach(variable IN ITEMS AC_IMPORT AC_PORT SL_REPLAY TOWN_SCRIPT TITLE_SCRIPT AC_VERSION OUT_DIR)
    if("${${variable}}" STREQUAL "")
        message(FATAL_ERROR "ac_port_window_replay: ${variable} is not set")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUT_DIR}")
set(package "${OUT_DIR}/${AC_VERSION}.slpack")
execute_process(COMMAND "${AC_IMPORT}" "${AC_DISC}" "${package}" "${OUT_DIR}/cache"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ac_port_window_replay: ac-import failed (${result}):\n${output}${errors}")
endif()
message("ac-import: ${output}")

foreach(name IN ITEMS town title)
    string(TOUPPER "${name}" upper)
    set(${name} "${OUT_DIR}/${name}_replay.slir")
    file(REMOVE "${${name}}")
    execute_process(COMMAND "${SL_REPLAY}" build "${${upper}_SCRIPT}" "${${name}}"
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
    if(NOT result EQUAL 0 OR NOT EXISTS "${${name}}")
        message(FATAL_ERROR "ac_port_window_replay: sl-replay failed (${result}):\n${output}${errors}")
    endif()
    message("${output}")
endforeach()

set(user "${OUT_DIR}/user")
file(REMOVE_RECURSE "${user}")

set(count "[0-9]+")
set(line_pattern "ac_port: [a-z]+ tick ${count} retrace ${count} gx ${count} dvd ${count} ${count} hash [0-9a-f]+")
string(APPEND line_pattern " instances ${count} compiles ${count} holes ${count}/${count} cpu ${count}")
string(APPEND line_pattern " refused ${count} views ${count} invalid ${count} problems ${count} overflows ${count}")
string(APPEND line_pattern " unowned ${count} strays ${count} gx-errors ${count} unmapped ${count} outside ${count}")
string(APPEND line_pattern " nested ${count} refused-frames ${count} audio ${count} loud ${count} dsp ${count}")
string(APPEND line_pattern " sound [0-9a-f]+ scene ${count}")

# Runs ac_port with the arguments given, headless unless they hold --window, which runs offscreen with the user
# directories under OUT_DIR/user, setting result, output, errors, lines, the play and replay lines in order, and
# seconds, the run's time.
macro(run_port)
    string(TIMESTAMP started "%s")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy
                            "VK_DRIVER_FILES=${AC_LAVAPIPE_ICD}" "${AC_PORT}" --package "${package}" ${ARGN}
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 600)
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

# The town replay, headless, then in a window, unpaced.
run_port(--replay "${town}")
set(headless "${lines}")
message("town headless: exit ${result}, ${seconds} s:\n${headless}")
list(LENGTH headless found)
if(NOT result EQUAL 0 OR NOT found EQUAL 2)
    tail_of("${errors}" errors)
    message(FATAL_ERROR "ac_port_window_replay: the headless town replay did not print a play and a replay line:\n"
                        "${errors}")
endif()
run_port(--window --user-dir "${user}" --unpaced --replay "${town}" --set sonar.video.render_scale=1)
string(REGEX MATCH "ac_port: window [^\n]*" window_line "${output}")
message("town window: exit ${result}, ${seconds} s:\n${lines}\n${window_line}")
if(NOT result EQUAL 0 OR window_line STREQUAL "")
    tail_of("${errors}" errors)
    message(FATAL_ERROR "ac_port_window_replay: the window's town replay did not end with the window line:\n${errors}")
endif()
if(NOT lines STREQUAL headless)
    message(FATAL_ERROR "ac_port_window_replay: the window's play and replay lines are not the headless run's:\n"
                        "${lines}\n${headless}")
endif()
list(GET lines 1 replay_line)
string(FIND "${output}" "${replay_line}" replay_at)
string(FIND "${output}" "${window_line}" window_at)
if(window_at LESS replay_at)
    message(FATAL_ERROR "ac_port_window_replay: the window line does not follow the replay line:\n${output}")
endif()
if(NOT window_line MATCHES " cards - - target 640x480$")
    message(FATAL_ERROR "ac_port_window_replay: the window's town replay has a card in a slot, or a target that is "
                        "not the logical size at scale 1:\n${window_line}")
endif()

# The title replay, headless, then in a window, paced.
run_port(--replay "${title}")
set(headless "${lines}")
message("title headless: exit ${result}, ${seconds} s:\n${headless}")
list(LENGTH headless found)
if(NOT result EQUAL 0 OR NOT found EQUAL 2)
    tail_of("${errors}" errors)
    message(FATAL_ERROR "ac_port_window_replay: the headless title replay did not print a play and a replay line:\n"
                        "${errors}")
endif()
list(GET headless 1 headless_replay)
if(NOT headless_replay MATCHES " scene 28$")
    message(FATAL_ERROR "ac_port_window_replay: the title replay did not end in scene 28:\n${headless_replay}")
endif()
run_port(--window --user-dir "${user}" --replay "${title}" --set sonar.video.render_scale=1)
string(REGEX MATCH "ac_port: window [^\n]*" window_line "${output}")
message("title window: exit ${result}, ${seconds} s:\n${lines}\n${window_line}")
if(NOT result EQUAL 0 OR window_line STREQUAL "")
    tail_of("${errors}" errors)
    message(FATAL_ERROR "ac_port_window_replay: the window's title replay did not end with the window line:\n${errors}")
endif()
if(NOT lines STREQUAL headless)
    message(FATAL_ERROR "ac_port_window_replay: the window's title replay lines are not the headless run's:\n"
                        "${lines}\n${headless}")
endif()
if(seconds LESS 20)
    message(FATAL_ERROR "ac_port_window_replay: the paced title replay took ${seconds} s, less than its 20 s")
endif()
if(NOT window_line MATCHES " blended [1-9][0-9]* ")
    message(FATAL_ERROR "ac_port_window_replay: the paced title replay blended no present:\n${window_line}")
endif()

# The title replay in a window, paced, with interpolation off.
run_port(--window --user-dir "${user}" --replay "${title}" --set sonar.video.interpolation=false
         --set sonar.video.render_scale=1)
string(REGEX MATCH "ac_port: window ticks ([0-9]+) frames ([0-9]+) blended ([0-9]+) [^\n]*" window_line "${output}")
set(ticks "${CMAKE_MATCH_1}")
set(frames "${CMAKE_MATCH_2}")
set(blended "${CMAKE_MATCH_3}")
message("title window, interpolation off: exit ${result}, ${seconds} s:\n${lines}\n${window_line}")
if(NOT result EQUAL 0 OR window_line STREQUAL "")
    tail_of("${errors}" errors)
    message(FATAL_ERROR "ac_port_window_replay: the title replay with interpolation off did not end with the window "
                        "line:\n${errors}")
endif()
if(NOT lines STREQUAL headless)
    message(FATAL_ERROR "ac_port_window_replay: the title replay's lines with interpolation off are not the headless "
                        "run's:\n${lines}\n${headless}")
endif()
if(NOT blended EQUAL 0 OR frames GREATER ticks)
    message(FATAL_ERROR "ac_port_window_replay: with interpolation off the window blended ${blended} presents or "
                        "presented more frames than ticks:\n${window_line}")
endif()
set(settings_file "${user}/config/settings.toml")
if(EXISTS "${settings_file}")
    file(READ "${settings_file}" settings)
    if(settings MATCHES "interpolation")
        message(FATAL_ERROR "ac_port_window_replay: the settings file kept the --set's interpolation:\n${settings}")
    endif()
endif()

# Usage errors.
foreach(arguments IN ITEMS "--window;--user-dir;${user};--unpaced" "--unpaced;--replay;${title}"
                           "--window;--user-dir;${user};--replay;${title};--set;sonar.system.progressive_scan=true")
    run_port(${arguments})
    message("${arguments}: exit ${result}")
    if(NOT result EQUAL 2)
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_window_replay: ${arguments} is not a usage error:\n${errors}")
    endif()
endforeach()
