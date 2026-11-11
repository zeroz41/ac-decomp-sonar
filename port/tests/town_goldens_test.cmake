# The town's goldens along the town replay (port/NOTES.md, Goldens; sonarlib docs/roadmap.md, M6 step 9f4a):
#   cmake -DAC_IMPORT=<ac-import> -DAC_PORT=<ac_port> -DSL_REPLAY=<sl-replay> -DTOWN_SCRIPT=<town_replay.txt>
#         -DTITLE_SCRIPT=<title_replay.txt> -DAC_DISC=<disc> -DAC_VERSION=<version> -DSCENES=<town.scenes>
#         -DOUT_DIR=<dir> -DAC_LAVAPIPE_ICD=<lavapipe's ICD> -P town_goldens_test.cmake
# ac-import writes the package from the player's disc into OUT_DIR, and sl-replay builds the two scripts into
# recordings there. Each run is ac_port --replay --goldens, on the GPU sink through SDL's offscreen video driver and
# lavapipe's Vulkan, with SDL's dummy audio driver: nothing opens on the desktop and nothing plays. The runs are
# compared with each other, as no Dolphin frame of the town is taken before step 9f4c.
#
# The town replay with the town's scenes, into port: it must exit 0 with the play line and the goldens line for every
# scene, its scenes.tsv must list the scenes in the file's order, each the VI's crop, 598 x 464 at (20, 6), and each
# frame must be of that size. Then in a 16:9 display with widescreen off, the setting's off switch, into
# port-16x9-off: every frame must be the first run's byte for byte. Then in a 21:9 display, 1120 x 480, into
# port-21x9: each frame must be 1078 x 464 and each window picture 1120 x 480. Then the same with the UI scale at 50
# into port-21x9-ui50: its sizes are port-21x9's, each scene the scenes file names as holding the game's 2D must
# differ from port-21x9's in its frame and in its window picture, and every other scene must be port-21x9's in both,
# byte for byte, so the setting reaches the town's 2D and nothing else.
#
# The ending: the title replay with the town's scenes, which ends at tick 1201, before the first, into title: it must
# exit 1 with the play line and the replay line at tick 1201 and no goldens line, write no scene and name train_rover
# as not captured. The town replay with --ticks 24001 and --state-hashes, into limit, then the same in the 21:9 display,
# into limit-21x9: each must exit 1 with the play line and the limit line at tick 24001 and no goldens line, its
# scenes.tsv must hold the scenes whose retrace comes before the limit, each frame and window picture the full run's
# byte for byte, and it must name the first scene after the limit as not captured. There, in the town at block (3,2),
# the 21:9 display's sides hold actors the game culls, which the culling hook draws (NOTES.md, The window): the 21:9
# run's limit line must be the 1x run's in every field but the frame's hash and the kit's counts of what it drew, which
# are hash, instances, compiles, holes and cpu, it must count more instances, and its state hashes must be the 1x
# run's, as what the hook draws is no part of the game's state. The replay's own end, on the beach, shows nothing more
# at the sides.
#
# It prints every line it checks. When AC_DISC names no disc, there is no lavapipe, or the disc is another version than
# GAFE01_00, whose retraces the scenes are, it prints the line the test's SKIP_REGULAR_EXPRESSION matches and returns.
cmake_minimum_required(VERSION 3.25)

if("${AC_DISC}" STREQUAL "")
    message("ac_port_town_goldens: skipped, AC_DISC is not set")
    return()
endif()
if("${AC_LAVAPIPE_ICD}" STREQUAL "")
    message("ac_port_town_goldens: skipped, no lavapipe ICD for the offscreen driver's Vulkan")
    return()
endif()
if(NOT AC_VERSION STREQUAL "GAFE01_00")
    message("ac_port_town_goldens: skipped, the scenes' retraces are GAFE01_00's, not ${AC_VERSION}'s")
    return()
endif()
foreach(variable IN ITEMS AC_IMPORT AC_PORT SL_REPLAY TOWN_SCRIPT TITLE_SCRIPT SCENES OUT_DIR)
    if("${${variable}}" STREQUAL "")
        message(FATAL_ERROR "ac_port_town_goldens: ${variable} is not set")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUT_DIR}")
set(package "${OUT_DIR}/${AC_VERSION}.slpack")
execute_process(COMMAND "${AC_IMPORT}" "${AC_DISC}" "${package}" "${OUT_DIR}/cache"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ac_port_town_goldens: ac-import failed (${result}):\n${output}${errors}")
endif()
message("ac-import: ${output}")

foreach(name IN ITEMS town title)
    string(TOUPPER "${name}" upper)
    set(${name} "${OUT_DIR}/${name}_replay.slir")
    file(REMOVE "${${name}}")
    execute_process(COMMAND "${SL_REPLAY}" build "${${upper}_SCRIPT}" "${${name}}"
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
    if(NOT result EQUAL 0 OR NOT EXISTS "${${name}}")
        message(FATAL_ERROR "ac_port_town_goldens: sl-replay failed (${result}):\n${output}${errors}")
    endif()
    message("${output}")
endforeach()

# The scenes file's names, in its order, and those its 2D line names as holding the game's 2D.
file(STRINGS "${SCENES}" scene_lines REGEX "^[a-z_0-9]+ ")
set(scenes "")
foreach(line IN LISTS scene_lines)
    string(REGEX REPLACE " .*$" "" scene "${line}")
    list(APPEND scenes "${scene}")
endforeach()
list(LENGTH scenes scene_count)
file(STRINGS "${SCENES}" with_2d REGEX "^# 2D: ")
string(REGEX REPLACE "^# 2D: " "" with_2d "${with_2d}")
string(REPLACE " " ";" with_2d "${with_2d}")
foreach(scene IN LISTS with_2d)
    if(NOT scene IN_LIST scenes)
        message(FATAL_ERROR "ac_port_town_goldens: the 2D line's ${scene} is not a scene")
    endif()
endforeach()
list(LENGTH with_2d count_2d)
string(REPLACE ";" " " shown "${scenes}")
message("${SCENES}: ${scene_count} scenes: ${shown}")
string(REPLACE ";" " " shown "${with_2d}")
message("holding 2D: ${count_2d}: ${shown}")
if(scene_count LESS 2 OR count_2d LESS 1 OR NOT count_2d LESS scene_count)
    message(FATAL_ERROR "ac_port_town_goldens: the scenes file's scenes or its 2D line are not read")
endif()

set(count "[0-9]+")
set(line_pattern "ac_port: [a-z]+ tick ${count} retrace ${count} gx ${count} dvd ${count} ${count} hash [0-9a-f]+")
string(APPEND line_pattern " instances ${count} compiles ${count} holes ${count}/${count} cpu ${count}")
string(APPEND line_pattern " refused ${count} views ${count} invalid ${count} problems ${count} overflows ${count}")
string(APPEND line_pattern " unowned ${count} strays ${count} gx-errors ${count} unmapped ${count} outside ${count}")
string(APPEND line_pattern " nested ${count} refused-frames ${count} audio ${count} loud ${count} dsp ${count}")
string(APPEND line_pattern " sound [0-9a-f]+ scene ${count}")
set(goldens_pattern "ac_port: goldens scenes ${count} frames ${count}")

# One run: ac_port --replay <recording> --goldens with the options into OUT_DIR/<name>, setting result, errors, lines,
# the report lines in order, goldens, the goldens line or "", and seconds, the run's time. It prints them.
macro(run_goldens name recording)
    file(REMOVE_RECURSE "${OUT_DIR}/${name}")
    file(MAKE_DIRECTORY "${OUT_DIR}/${name}")
    string(TIMESTAMP started "%s")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy
                            "VK_DRIVER_FILES=${AC_LAVAPIPE_ICD}" "${AC_PORT}" --package "${package}" --replay
                            "${recording}" --goldens "${SCENES}" "${OUT_DIR}/${name}" ${ARGN}
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 1200)
    string(TIMESTAMP ended "%s")
    math(EXPR seconds "${ended} - ${started}")
    string(REGEX MATCHALL "${line_pattern}" lines "${output}")
    string(REGEX MATCH "${goldens_pattern}" goldens "${output}")
    set(shown "")
    foreach(option ${ARGN})
        string(APPEND shown " ${option}")
    endforeach()
    message("${name}:${shown} exit ${result}, ${seconds} s:")
    foreach(line IN LISTS lines)
        message("  ${line}")
    endforeach()
    if(NOT goldens STREQUAL "")
        message("  ${goldens}")
    endif()
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

# A run that must write every scene: exit 0, the play line alone and the goldens line for every scene.
macro(run_all name recording)
    run_goldens(${name} "${recording}" ${ARGN})
    list(LENGTH lines found)
    if(NOT result EQUAL 0 OR NOT found EQUAL 1 OR NOT lines MATCHES "^ac_port: play "
       OR NOT goldens MATCHES "^ac_port: goldens scenes ${scene_count} frames ")
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_town_goldens: the ${name} run did not write every scene:\n${errors}")
    endif()
endmacro()

# The scenes a run wrote, from its scenes.tsv, into the caller's `written`, and their lines into `written_lines`.
function(scenes_of name)
    set(written "")
    set(written_lines "")
    if(EXISTS "${OUT_DIR}/${name}/scenes.tsv")
        file(STRINGS "${OUT_DIR}/${name}/scenes.tsv" written_lines)
    endif()
    foreach(line IN LISTS written_lines)
        string(REGEX REPLACE "\t.*$" "" scene "${line}")
        list(APPEND written "${scene}")
    endforeach()
    set(written "${written}" PARENT_SCOPE)
    set(written_lines "${written_lines}" PARENT_SCOPE)
endfunction()

# Whether a file of a run, a scene's frame or its window picture, is another run's, byte for byte, into the caller's
# `same`.
function(same_file name other file)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${OUT_DIR}/${name}/${file}"
                            "${OUT_DIR}/${other}/${file}"
                    RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
    if(result EQUAL 0)
        set(same TRUE PARENT_SCOPE)
    else()
        set(same FALSE PARENT_SCOPE)
    endif()
endfunction()

# Every scene of a run has a file, its frame (suffix .ppm) or its window picture (.window.ppm), of the size given as
# "<width> <height>".
function(sizes name suffix size)
    string(HEX "P6\n${size}\n255\n" expected)
    string(LENGTH "${expected}" length)
    math(EXPR bytes "${length} / 2")
    foreach(scene IN LISTS scenes)
        set(header "")
        if(EXISTS "${OUT_DIR}/${name}/${scene}${suffix}")
            file(READ "${OUT_DIR}/${name}/${scene}${suffix}" header LIMIT ${bytes} HEX)
        endif()
        if(NOT header STREQUAL expected)
            message(FATAL_ERROR "ac_port_town_goldens: ${name}/${scene}${suffix} is not ${size}")
        endif()
    endforeach()
    message("${name}: ${scene_count} ${suffix} files, each ${size}")
endfunction()

# The town replay with every scene, into port. Its scenes.tsv lists them in the file's order, each the VI's crop.
run_all(port "${town}")
scenes_of(port)
if(NOT written STREQUAL scenes)
    message(FATAL_ERROR "ac_port_town_goldens: port's scenes.tsv lists other scenes than the file's, in its order:\n"
                        "${written}")
endif()
foreach(line IN LISTS written_lines)
    if(NOT line MATCHES "^[a-z_0-9]+\t[0-9]+\t20\t6\t598\t464$")
        message(FATAL_ERROR "ac_port_town_goldens: port's scene is not the VI's crop, 598 x 464 at (20, 6):\n${line}")
    endif()
endforeach()
string(REPLACE ";" "\n  " shown "${written_lines}")
message("port's scenes.tsv:\n  ${shown}")
sizes(port ".ppm" "598 464")

# Widescreen off in a 16:9 display, the setting's off switch: the first run's frames, byte for byte.
run_all(port-16x9-off "${town}" --display 854x480 --set sonar.video.widescreen=false)
foreach(scene IN LISTS scenes)
    same_file(port-16x9-off port "${scene}.ppm")
    if(NOT same)
        message(FATAL_ERROR "ac_port_town_goldens: port-16x9-off/${scene}.ppm is not port's, byte for byte")
    endif()
endforeach()
message("port-16x9-off: ${scene_count} frames, each port's byte for byte")

# A 21:9 display of the game's height adds 480 columns, half on each side, to the VI's crop.
run_all(port-21x9 "${town}" --display 1120x480)
sizes(port-21x9 ".ppm" "1078 464")
sizes(port-21x9 ".window.ppm" "1120 480")

# The UI scale at 50 in the 21:9 display: the scenes holding 2D differ from port-21x9's, frame and window picture, and
# the others are the same, both byte for byte.
run_all(port-21x9-ui50 "${town}" --display 1120x480 --set sonar.video.ui_scale=50)
sizes(port-21x9-ui50 ".ppm" "1078 464")
sizes(port-21x9-ui50 ".window.ppm" "1120 480")
set(differing "")
foreach(scene IN LISTS scenes)
    foreach(file IN ITEMS "${scene}.ppm" "${scene}.window.ppm")
        same_file(port-21x9-ui50 port-21x9 "${file}")
        if(scene IN_LIST with_2d AND same)
            message(FATAL_ERROR "ac_port_town_goldens: port-21x9-ui50/${file} is port-21x9's: the UI scale did not "
                                "reach the scene's 2D")
        elseif(NOT scene IN_LIST with_2d AND NOT same)
            message(FATAL_ERROR "ac_port_town_goldens: port-21x9-ui50/${file} is not port-21x9's, byte for byte, "
                                "though the scene holds no 2D")
        endif()
    endforeach()
endforeach()
math(EXPR without_2d "${scene_count} - ${count_2d}")
message("port-21x9-ui50: the ${count_2d} scenes holding 2D differ from port-21x9's, frame and window picture; the "
        "${without_2d} others are port-21x9's in both, byte for byte")

# The replay ends first: the title replay ends at tick 1201, before the first scene.
run_goldens(title "${title}")
list(LENGTH lines found)
set(replay_line "")
if(found EQUAL 2)
    list(GET lines 1 replay_line)
endif()
scenes_of(title)
if(NOT result EQUAL 1 OR NOT lines MATCHES "^ac_port: play " OR NOT replay_line MATCHES "^ac_port: replay tick 1201 "
   OR NOT goldens STREQUAL "" OR NOT written STREQUAL "")
    tail_of("${errors}" errors)
    message(FATAL_ERROR "ac_port_town_goldens: the title run did not end with the replay at tick 1201, exit 1 and no "
                        "scene:\n${errors}")
endif()
list(GET scenes 0 first)
if(NOT errors MATCHES "scene ${first} was not captured")
    tail_of("${errors}" errors)
    message(FATAL_ERROR "ac_port_town_goldens: the title run does not name ${first} as not captured:\n${errors}")
endif()
message("title: no scene written; ${first} named as not captured")

# The limit comes first: at tick 24001, where the replay ended before step 9f4a, the runs have written the scenes whose
# retrace comes before it, and the first after it is left.
set(limit_tick 24001)
set(before_limit "")
set(left "")
foreach(line IN LISTS scene_lines)
    if(NOT line MATCHES "^([a-z_0-9]+) (.* )?retrace=([0-9a-f]+)( |$)")
        message(FATAL_ERROR "ac_port_town_goldens: a scene gives no retrace:\n${line}")
    endif()
    set(scene "${CMAKE_MATCH_1}")
    math(EXPR retrace "0x${CMAKE_MATCH_3}")
    if(retrace LESS limit_tick)
        list(APPEND before_limit "${scene}")
    elseif(left STREQUAL "")
        set(left "${scene}")
    endif()
endforeach()
list(LENGTH before_limit before_count)
if(before_count EQUAL 0 OR left STREQUAL "")
    message(FATAL_ERROR "ac_port_town_goldens: no scene comes before tick ${limit_tick}, or none after it")
endif()

# A run the limit stops, into OUT_DIR/<name>, with its state hashes into OUT_DIR/<name>.hashes: exit 1, the play line,
# the limit line at the limit's tick and no goldens line, the scenes before the limit, each frame and window picture
# the full run's, byte for byte, and the first scene after it named as not captured. It sets limit_line and hashes,
# the state hashes' text.
macro(run_limit name full)
    file(REMOVE "${OUT_DIR}/${name}.hashes")
    run_goldens(${name} "${town}" --ticks ${limit_tick} --state-hashes "${OUT_DIR}/${name}.hashes" ${ARGN})
    list(LENGTH lines found)
    set(limit_line "")
    if(found EQUAL 2)
        list(GET lines 1 limit_line)
    endif()
    scenes_of(${name})
    if(NOT result EQUAL 1 OR NOT lines MATCHES "^ac_port: play "
       OR NOT limit_line MATCHES "^ac_port: limit tick ${limit_tick} " OR NOT goldens STREQUAL ""
       OR NOT written STREQUAL before_limit)
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_town_goldens: the ${name} run did not end with the limit at tick ${limit_tick}, "
                            "exit 1 and the scenes before it:\n${written}\n${errors}")
    endif()
    if(NOT errors MATCHES "scene ${left} was not captured")
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_town_goldens: the ${name} run does not name ${left} as not captured:\n${errors}")
    endif()
    foreach(scene IN LISTS before_limit)
        foreach(file IN ITEMS "${scene}.ppm" "${scene}.window.ppm")
            if(EXISTS "${OUT_DIR}/${full}/${file}")
                same_file(${name} ${full} "${file}")
                if(NOT same)
                    message(FATAL_ERROR "ac_port_town_goldens: ${name}/${file} is not ${full}'s, byte for byte")
                endif()
            endif()
        endforeach()
    endforeach()
    set(hashes "")
    if(EXISTS "${OUT_DIR}/${name}.hashes")
        file(READ "${OUT_DIR}/${name}.hashes" hashes)
    endif()
    if(NOT hashes MATCHES "^0 [0-9a-f]+\n")
        message(FATAL_ERROR "ac_port_town_goldens: the ${name} run wrote no state hashes")
    endif()
    message("${name}: ${before_count} scenes written, each ${full}'s byte for byte; ${left} named as not captured")
endmacro()

# The play and limit lines without the frame's hash and the kit's counts of what it drew, which a wider display
# changes, into out.
function(without_drawn lines out)
    string(REGEX REPLACE " hash [0-9a-f]+ instances [0-9]+ compiles [0-9]+ holes [0-9]+/[0-9]+ cpu [0-9]+ "
                         " hash - instances - compiles - holes - cpu - " lines "${lines}")
    set(${out} "${lines}" PARENT_SCOPE)
endfunction()

run_limit(limit port)
set(narrow_lines "${lines}")
set(narrow_line "${limit_line}")
set(narrow_hashes "${hashes}")
run_limit(limit-21x9 port-21x9 --display 1120x480)
without_drawn("${narrow_lines}" narrow_simulated)
without_drawn("${lines}" wide_simulated)
if(narrow_simulated STREQUAL narrow_lines OR NOT wide_simulated STREQUAL narrow_simulated)
    message(FATAL_ERROR "ac_port_town_goldens: limit-21x9's lines are not limit's outside the frame's hash and the "
                        "kit's counts:\n${lines}\n${narrow_lines}")
endif()
string(REGEX MATCH " instances ([0-9]+) " found "${narrow_line}")
set(narrow_instances "${CMAKE_MATCH_1}")
string(REGEX MATCH " instances ([0-9]+) " found "${limit_line}")
set(wide_instances "${CMAKE_MATCH_1}")
if(NOT wide_instances GREATER narrow_instances)
    message(FATAL_ERROR "ac_port_town_goldens: limit-21x9 counts ${wide_instances} instances at tick ${limit_tick}, "
                        "no more than limit's ${narrow_instances}: the culling hook drew nothing at the sides")
endif()
if(NOT hashes STREQUAL narrow_hashes)
    message(FATAL_ERROR "ac_port_town_goldens: limit-21x9's state hashes are not limit's: what the culling hook drew "
                        "changed the game's state")
endif()
string(REGEX MATCHALL "\n" boundaries "${hashes}")
list(LENGTH boundaries boundaries)
message("limit-21x9: ${wide_instances} instances at tick ${limit_tick}, limit ${narrow_instances}; its ${boundaries} "
        "state hashes limit's")
