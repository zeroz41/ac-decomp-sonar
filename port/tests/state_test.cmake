# The town replay's state hashes and dumps (port/NOTES.md, State hashes; sonarlib docs/roadmap.md, M6 steps 9f3b, 9c1,
# 9c3 and 9e1):
#   cmake -DAC_IMPORT=<ac-import> -DAC_PORT=<ac_port> -DSL_REPLAY=<sl-replay> -DSL_DUMP_COMPARE=<sl-dump-compare>
#         -DSYMBOLS=<ac_port.slsym> -DTOWN_SCRIPT=<town_replay.txt> -DTITLE_SCRIPT=<title_replay.txt> -DAC_DISC=<disc>
#         -DAC_VERSION=<version> -DOUT_DIR=<dir> -DAC_LAVAPIPE_ICD=<lavapipe's ICD> -P state_test.cmake
# ac-import writes the package from the player's disc into OUT_DIR, and sl-replay builds the two scripts into
# recordings there. Each window and measured run shows the game on SDL's offscreen video driver, through lavapipe's
# Vulkan, with SDL's dummy audio driver, the window runs keeping their settings under OUT_DIR/user: nothing opens on
# the desktop, nothing plays and no player's file is read or written.
#
# The town replay runs headless twice with --state-hashes, --dump and --dump-tick 20000. Each must exit 0 with the
# play line and the replay line at tick 24001 in the town, the same in both, write a line for each tick boundary, its
# ticks consecutive from 0, and write the dump. The two files must be equal, and sl-dump-compare --symbols on the two
# dumps must exit 0 with no variable and no arena differing.
#
# The town replay then runs with --window --unpaced twice, uncapped with --set sonar.video.pacing=immediate and with
# interpolation off with --set sonar.video.interpolation=false, each with --state-hashes, at render scale 1 and with
# its window resized to 1280x720 at tick 6000, 960x720 at 12000 and 1680x720 at 18000: 16:9, 4:3 and 21:9. Each must
# exit 0 with the window line, which must end with the target of the last size, 1120x480; its stderr must report each
# resize taken; and the headless file must be a prefix of each run's file, as the picture's size is no part of the
# game's state. A wider window shows what the game culls at the sides of its frame (NOTES.md, The window), so its
# frames hold more than a headless run's: its play and replay lines must be the headless run's in every field but the
# frame's hash and the kit's counts of what it drew, which are hash, instances, compiles, holes and cpu, and its replay
# line, at 21:9, must count more instances than the headless run's, which shows the culling hook drew.
#
# The plant: the town replay headless with --state-hashes, --plant 19999, --dump and --dump-tick 20000. It must exit 0
# with the clean runs' lines, its hashes must equal theirs to tick 19999 and differ from each one's from tick 20000 on,
# and sl-dump-compare on its dump and a clean one must exit 1, naming common_data at +0x1c, the save's pad_1C[0], as
# the first difference, with one variable differing.
#
# The picture's settings (M6 steps 9e1 and 9e2): the title replay headless with --state-hashes, which must exit 0 with
# the replay line at tick 1201 in scene 28, then with --window --unpaced, interpolation off, --resize 1 640x480 and
# --set for SMAA, FSR 1, RCAS, supersampling at 2 and multisampling at 4, the count changed at the run's first present.
# It must exit 0 with the window line, which must end with the target 1280x960, twice the window's size a side; its
# play and replay lines must be the headless run's, whole, as a 4:3 window draws what a headless run draws; and the
# headless file must be a prefix of its file, as no setting of the picture is part of the game's state.
#
# The title replay measured past its end, --measure --replay with --ticks 1500: it must exit 0 with the play line, the
# measure line, the presents line and the replay line at tick 1201 in scene 28, and no measured line.
#
# Usage errors: --state-hashes and --dump in a window without --replay or --record, --dump without --dump-tick and the
# reverse, --plant without --state-hashes, and --measure --replay without --ticks must each exit 2 and write no file.
#
# It prints every line it checks. When AC_DISC names no disc or there is no lavapipe it prints the line the test's
# SKIP_REGULAR_EXPRESSION matches and returns.
cmake_minimum_required(VERSION 3.25)

if("${AC_DISC}" STREQUAL "")
    message("ac_port_state: skipped, AC_DISC is not set")
    return()
endif()
if("${AC_LAVAPIPE_ICD}" STREQUAL "")
    message("ac_port_state: skipped, no lavapipe ICD for the offscreen driver's Vulkan surface")
    return()
endif()
foreach(variable IN ITEMS AC_IMPORT AC_PORT SL_REPLAY SL_DUMP_COMPARE SYMBOLS TOWN_SCRIPT TITLE_SCRIPT AC_VERSION
                          OUT_DIR)
    if("${${variable}}" STREQUAL "")
        message(FATAL_ERROR "ac_port_state: ${variable} is not set")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUT_DIR}")
set(package "${OUT_DIR}/${AC_VERSION}.slpack")
execute_process(COMMAND "${AC_IMPORT}" "${AC_DISC}" "${package}" "${OUT_DIR}/cache"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ac_port_state: ac-import failed (${result}):\n${output}${errors}")
endif()
message("ac-import: ${output}")

foreach(name IN ITEMS town title)
    string(TOUPPER "${name}" upper)
    set(${name} "${OUT_DIR}/${name}_replay.slir")
    file(REMOVE "${${name}}")
    execute_process(COMMAND "${SL_REPLAY}" build "${${upper}_SCRIPT}" "${${name}}"
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
    if(NOT result EQUAL 0 OR NOT EXISTS "${${name}}")
        message(FATAL_ERROR "ac_port_state: sl-replay failed (${result}):\n${output}${errors}")
    endif()
    message("${output}")
endforeach()

set(user "${OUT_DIR}/user")
set(refused_hashes "${OUT_DIR}/refused.txt")
set(refused_dumps "${OUT_DIR}/refused")
file(REMOVE_RECURSE "${user}" "${refused_dumps}")
file(REMOVE "${refused_hashes}")

# The tick of the dumps, and the plant's, the tick before it.
set(dump_tick 20000)
math(EXPR plant_tick "${dump_tick} - 1")

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

# Runs ac_port with the arguments given, headless unless they hold --window, which runs offscreen with the user
# directories under OUT_DIR/user, setting result, output, errors, lines, the play and replay lines in order,
# window_line, and seconds, the run's time.
macro(run_port)
    string(TIMESTAMP started "%s")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy
                            "VK_DRIVER_FILES=${AC_LAVAPIPE_ICD}" "${AC_PORT}" --package "${package}" ${ARGN}
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 600)
    string(TIMESTAMP ended "%s")
    math(EXPR seconds "${ended} - ${started}")
    string(REGEX MATCHALL "${line_pattern}" lines "${output}")
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

# Fails unless the state hashes file holds a line for each tick boundary, the tick and the hash as 16 hex digits, its
# ticks consecutive from 0, and sets out to its contents.
function(read_hashes name file out)
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "ac_port_state: ${name} wrote no state hashes")
    endif()
    file(STRINGS "${file}" hash_lines)
    set(expected 0)
    foreach(hash_line IN LISTS hash_lines)
        set(tick "")
        if(hash_line MATCHES "^([0-9]+) [0-9a-f]+$")
            set(tick "${CMAKE_MATCH_1}")
        endif()
        if(NOT tick STREQUAL expected)
            message(FATAL_ERROR "ac_port_state: ${name}'s state hashes have no line for tick ${expected}:\n"
                                "${hash_line}")
        endif()
        string(LENGTH "${hash_line}" length)
        string(LENGTH "${tick}" tick_length)
        math(EXPR hash_length "${length} - ${tick_length} - 1")
        if(NOT hash_length EQUAL 16)
            message(FATAL_ERROR "ac_port_state: ${name}'s hash at tick ${expected} is not 16 hex digits:\n${hash_line}")
        endif()
        math(EXPR expected "${expected} + 1")
    endforeach()
    if(expected EQUAL 0)
        message(FATAL_ERROR "ac_port_state: ${name}'s state hashes are empty")
    endif()
    message("${name}: ${expected} state hashes, ticks 0 to ${tick}")
    file(READ "${file}" text)
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# Fails unless the text `prefix` begins the text `text`.
function(check_prefix name prefix text)
    string(LENGTH "${prefix}" length)
    string(SUBSTRING "${text}" 0 ${length} start)
    if(NOT start STREQUAL prefix)
        message(FATAL_ERROR "ac_port_state: the headless run's state hashes do not begin ${name}'s")
    endif()
endfunction()

# Sets out to the play and replay lines without the frame's hash and the kit's counts of what it drew, which a wider
# display changes: hash, instances, compiles, holes and cpu.
function(without_drawn lines out)
    string(REGEX REPLACE " hash [0-9a-f]+ instances [0-9]+ compiles [0-9]+ holes [0-9]+/[0-9]+ cpu [0-9]+ "
                         " hash - instances - compiles - holes - cpu - " lines "${lines}")
    set(${out} "${lines}" PARENT_SCOPE)
endfunction()

# Sets out to the instances the replay line among the lines counts, or to nothing.
function(replay_instances lines out)
    set(instances "")
    foreach(line IN LISTS lines)
        if(line MATCHES "^ac_port: replay .* instances ([0-9]+) ")
            set(instances "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    set(${out} "${instances}" PARENT_SCOPE)
endfunction()

# Runs sl-dump-compare --symbols on two dumps, setting result and compared, its output.
macro(compare_dumps first second)
    execute_process(COMMAND "${SL_DUMP_COMPARE}" --symbols "${SYMBOLS}" "${first}" "${second}"
                    RESULT_VARIABLE result OUTPUT_VARIABLE compared ERROR_VARIABLE errors)
    message("sl-dump-compare: exit ${result}:\n${compared}${errors}")
endmacro()

# The town replay headless, twice, with the state hashes and a dump.
set(clean_runs "")
foreach(run IN ITEMS 1 2)
    set(dumps "${OUT_DIR}/dumps_${run}")
    set(hashes "${OUT_DIR}/hashes_${run}.txt")
    file(REMOVE_RECURSE "${dumps}")
    file(REMOVE "${hashes}")
    run_port(--replay "${town}" --state-hashes "${hashes}" --dump "${dumps}" --dump-tick ${dump_tick})
    message("town headless ${run}: exit ${result}, ${seconds} s:\n${lines}")
    list(LENGTH lines found)
    set(replay_line "")
    if(found EQUAL 2)
        list(GET lines 1 replay_line)
    endif()
    if(NOT result EQUAL 0 OR NOT replay_line MATCHES "^ac_port: replay tick 24001 .* scene 7$")
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_state: headless run ${run} did not end with the replay in the town at tick "
                            "24001:\n${errors}")
    endif()
    if(NOT EXISTS "${dumps}/tick${dump_tick}.sldump")
        message(FATAL_ERROR "ac_port_state: headless run ${run} wrote no dump of tick ${dump_tick}")
    endif()
    read_hashes("headless run ${run}" "${hashes}" hashes_${run})
    list(APPEND clean_runs "${lines}")
endforeach()
set(headless "${lines}")
list(GET clean_runs 0 1 first_lines)
list(GET clean_runs 2 3 second_lines)
if(NOT first_lines STREQUAL second_lines)
    message(FATAL_ERROR "ac_port_state: the headless runs' lines differ:\n${first_lines}\n${second_lines}")
endif()
if(NOT hashes_1 STREQUAL hashes_2)
    message(FATAL_ERROR "ac_port_state: the headless runs' state hashes differ")
endif()
set(clean_dump "${OUT_DIR}/dumps_1/tick${dump_tick}.sldump")
compare_dumps("${clean_dump}" "${OUT_DIR}/dumps_2/tick${dump_tick}.sldump")
if(NOT result EQUAL 0
   OR NOT compared MATCHES "\nsummary: 0 of [1-9][0-9]* variables differ; 0 of [1-9][0-9]* arenas differ\n")
    message(FATAL_ERROR "ac_port_state: the headless runs' dumps differ, or compared no variable or no arena")
endif()
without_drawn("${headless}" headless_simulated)
replay_instances("${headless}" headless_instances)
if(headless_simulated STREQUAL headless OR headless_instances STREQUAL "")
    message(FATAL_ERROR "ac_port_state: the headless lines hold no hash and counts to leave out:\n${headless}")
endif()

# The town replay in a window, unpaced: uncapped, then with interpolation off, each resized as it runs.
set(resizes "6000 1280x720" "12000 960x720" "18000 1680x720")
set(resize_arguments "")
foreach(resize IN LISTS resizes)
    string(REPLACE " " ";" resize_argument "${resize}")
    list(APPEND resize_arguments --resize ${resize_argument})
endforeach()
foreach(setting IN ITEMS sonar.video.pacing=immediate sonar.video.interpolation=false)
    set(hashes "${OUT_DIR}/hashes_window.txt")
    file(REMOVE "${hashes}")
    run_port(--window --user-dir "${user}" --unpaced --replay "${town}" --state-hashes "${hashes}" --set "${setting}"
             --set sonar.video.render_scale=1 ${resize_arguments})
    message("town window, ${setting}: exit ${result}, ${seconds} s:\n${lines}\n${window_line}")
    if(NOT result EQUAL 0 OR window_line STREQUAL "")
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_state: the window's town replay with ${setting} did not end with the window "
                            "line:\n${errors}")
    endif()
    without_drawn("${lines}" simulated)
    if(NOT simulated STREQUAL headless_simulated)
        message(FATAL_ERROR "ac_port_state: the window's lines with ${setting} are not the headless run's outside the "
                            "frame's hash and the kit's counts:\n${lines}\n${headless}")
    endif()
    replay_instances("${lines}" instances)
    if(instances STREQUAL "" OR NOT instances GREATER headless_instances)
        message(FATAL_ERROR "ac_port_state: the window's replay with ${setting} ends at 21:9 with ${instances} "
                            "instances, no more than the headless run's ${headless_instances}: the culling hook drew "
                            "nothing at the sides")
    endif()
    message("the window run with ${setting}: ${instances} instances at 21:9, ${headless_instances} headless")
    string(REGEX MATCHALL "ac_port: resize [^\n]*" resized "${errors}")
    message("${resized}")
    foreach(resize IN LISTS resizes)
        if(NOT "ac_port: resize ${resize}: ok" IN_LIST resized)
            tail_of("${errors}" errors)
            message(FATAL_ERROR "ac_port_state: the window run with ${setting} did not resize at ${resize}:\n${errors}")
        endif()
    endforeach()
    if(NOT window_line MATCHES " target 1120x480$")
        message(FATAL_ERROR "ac_port_state: the window run with ${setting} did not end with the 21:9 window's "
                            "target, 1120x480:\n${window_line}")
    endif()
    read_hashes("the window run with ${setting}" "${hashes}" window_hashes)
    check_prefix("the window run's with ${setting}" "${hashes_1}" "${window_hashes}")
endforeach()

# The plant: one byte of the save changed at the boundary that begins the tick before the dump's, after its hash.
set(dumps "${OUT_DIR}/dumps_plant")
set(hashes "${OUT_DIR}/hashes_plant.txt")
file(REMOVE_RECURSE "${dumps}")
file(REMOVE "${hashes}")
run_port(--replay "${town}" --state-hashes "${hashes}" --plant ${plant_tick} --dump "${dumps}" --dump-tick ${dump_tick})
message("town headless, planted at ${plant_tick}: exit ${result}, ${seconds} s:\n${lines}")
if(NOT result EQUAL 0 OR NOT lines STREQUAL headless OR NOT EXISTS "${dumps}/tick${dump_tick}.sldump")
    tail_of("${errors}" errors)
    message(FATAL_ERROR "ac_port_state: the planted run did not end with the clean runs' lines and its dump:\n"
                        "${lines}\n${headless}\n${errors}")
endif()
read_hashes("the planted run" "${hashes}" plant_hashes)
# The hashes to the plant's tick, then those after it, line by line.
string(FIND "${hashes_1}" "\n${dump_tick} " clean_at)
string(FIND "${plant_hashes}" "\n${dump_tick} " plant_at)
if(clean_at LESS 0 OR NOT plant_at EQUAL clean_at)
    message(FATAL_ERROR "ac_port_state: the planted run's state hashes to tick ${plant_tick} are not the clean runs'")
endif()
string(SUBSTRING "${hashes_1}" 0 ${clean_at} clean_before)
string(SUBSTRING "${plant_hashes}" 0 ${plant_at} plant_before)
if(NOT plant_before STREQUAL clean_before)
    message(FATAL_ERROR "ac_port_state: the planted run's state hashes to tick ${plant_tick} are not the clean runs'")
endif()
math(EXPR clean_at "${clean_at} + 1")
string(SUBSTRING "${hashes_1}" ${clean_at} -1 clean_after)
string(SUBSTRING "${plant_hashes}" ${clean_at} -1 plant_after)
string(REGEX MATCHALL "[^\n]+" clean_after "${clean_after}")
string(REGEX MATCHALL "[^\n]+" plant_after "${plant_after}")
list(LENGTH clean_after clean_count)
list(LENGTH plant_after plant_count)
if(NOT plant_count EQUAL clean_count)
    message(FATAL_ERROR "ac_port_state: the planted run has ${plant_count} state hashes from tick ${dump_tick}, the "
                        "clean runs ${clean_count}")
endif()
foreach(clean_line plant_line IN ZIP_LISTS clean_after plant_after)
    if(clean_line STREQUAL plant_line)
        message(FATAL_ERROR "ac_port_state: the planted run's state hash is the clean runs' after the plant:\n"
                            "${plant_line}")
    endif()
endforeach()
message("the planted run's state hashes: the clean runs' to tick ${plant_tick}, then ${plant_count} that differ")
# The byte is common_data's at +0x1c, the save's pad_1C[0], on any host: the save is the first field and holds no
# pointer before it.
compare_dumps("${clean_dump}" "${dumps}/tick${dump_tick}.sldump")
if(NOT result EQUAL 1 OR NOT compared MATCHES "\nfirst difference: common_data at \\+0x1c of [0-9]+ bytes\n"
   OR NOT compared MATCHES "\nsummary: 1 of [0-9]+ variables differ; 0 of [0-9]+ arenas differ\n")
    message(FATAL_ERROR "ac_port_state: the planted dump's compare did not name common_data at +0x1c, alone")
endif()

# The picture's settings: the title replay headless, then in a 4:3 window with the post chain, supersampling at 2 and
# multisampling at 4.
set(hashes "${OUT_DIR}/hashes_title.txt")
file(REMOVE "${hashes}")
run_port(--replay "${title}" --state-hashes "${hashes}")
message("title headless: exit ${result}, ${seconds} s:\n${lines}")
list(LENGTH lines found)
set(replay_line "")
if(found EQUAL 2)
    list(GET lines 1 replay_line)
endif()
if(NOT result EQUAL 0 OR NOT replay_line MATCHES "^ac_port: replay tick 1201 .* scene 28$")
    tail_of("${errors}" errors)
    message(FATAL_ERROR "ac_port_state: the headless title replay did not end at tick 1201 in scene 28:\n${errors}")
endif()
read_hashes("the headless title replay" "${hashes}" title_hashes)
set(title_headless "${lines}")
set(hashes "${OUT_DIR}/hashes_title_window.txt")
file(REMOVE "${hashes}")
run_port(--window --user-dir "${user}" --unpaced --replay "${title}" --state-hashes "${hashes}"
         --set sonar.video.interpolation=false --set sonar.video.antialiasing=smaa --set sonar.video.upscale=fsr1
         --set sonar.video.sharpen=rcas --set sonar.video.supersampling=2 --set sonar.video.msaa=4x
         --resize 1 640x480)
message("title window, the picture's settings: exit ${result}, ${seconds} s:\n${lines}\n${window_line}")
if(NOT result EQUAL 0 OR window_line STREQUAL "")
    tail_of("${errors}" errors)
    message(FATAL_ERROR "ac_port_state: the window's title replay with the picture's settings did not end with the "
                        "window line:\n${errors}")
endif()
if(NOT lines STREQUAL title_headless)
    message(FATAL_ERROR "ac_port_state: the window's lines with the picture's settings are not the headless "
                        "run's:\n${lines}\n${title_headless}")
endif()
if(NOT window_line MATCHES " target 1280x960$")
    message(FATAL_ERROR "ac_port_state: the window run with the picture's settings did not end with the target twice "
                        "its 640x480 window's size, 1280x960:\n${window_line}")
endif()
read_hashes("the window run with the picture's settings" "${hashes}" window_hashes)
check_prefix("the window run's with the picture's settings" "${title_hashes}" "${window_hashes}")

# The title replay measured past its end.
run_port(--measure --replay "${title}" --ticks 1500)
string(REGEX MATCHALL "${measure_pattern}" measure_lines "${output}")
string(REGEX MATCHALL "${presents_pattern}" presents_lines "${output}")
string(REGEX MATCHALL "ac_port: measured [^\n]*" measured_lines "${output}")
message("title measured to 1500: exit ${result}, ${seconds} s:\n${measure_lines}\n${presents_lines}\n${lines}")
list(LENGTH measure_lines measures)
list(LENGTH presents_lines presents)
list(LENGTH lines found)
set(replay_line "")
if(found EQUAL 2)
    list(GET lines 1 replay_line)
endif()
if(NOT result EQUAL 0 OR NOT measures EQUAL 1 OR NOT presents EQUAL 1 OR NOT measured_lines STREQUAL ""
   OR NOT replay_line MATCHES "^ac_port: replay tick 1201 .* scene 28$")
    tail_of("${errors}" errors)
    message(FATAL_ERROR "ac_port_state: the measured title replay did not end with the measure, presents and replay "
                        "lines:\n${output}\n${errors}")
endif()
string(FIND "${output}" "${measure_lines}" measure_at)
string(FIND "${output}" "${presents_lines}" presents_at)
string(FIND "${output}" "${replay_line}" replay_at)
if(presents_at LESS measure_at OR replay_at LESS presents_at)
    message(FATAL_ERROR "ac_port_state: the measured title replay's lines are out of order:\n${output}")
endif()

# Usage errors.
foreach(arguments IN ITEMS "--window;--user-dir;${user};--state-hashes;${refused_hashes}"
                           "--window;--user-dir;${user};--dump;${refused_dumps};--dump-tick;10"
                           "--replay;${town};--state-hashes;${refused_hashes};--dump;${refused_dumps}"
                           "--replay;${town};--state-hashes;${refused_hashes};--dump-tick;10"
                           "--replay;${town};--plant;10;--dump;${refused_dumps};--dump-tick;10"
                           "--measure;--replay;${title};--state-hashes;${refused_hashes}")
    run_port(${arguments})
    message("${arguments}: exit ${result}")
    if(NOT result EQUAL 2 OR EXISTS "${refused_hashes}" OR EXISTS "${refused_dumps}")
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_state: ${arguments} is not a usage error, or wrote a file:\n${errors}")
    endif()
endforeach()
