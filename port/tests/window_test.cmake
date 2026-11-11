# Runs ac_port --window with its user directories under OUT_DIR/user, twice to play, then with bad assignments and
# twice with the memory cards (sonarlib docs/roadmap.md, M6 steps 6k, 6l, 6m, 11a, 11c, 9c1 and 9c2):
#   cmake -DAC_IMPORT=<ac-import> -DAC_PORT=<ac_port> -DAC_DISC=<disc> -DAC_VERSION=<version> -DOUT_DIR=<dir>
#         -DAC_LAVAPIPE_ICD=<lavapipe's ICD> -P window_test.cmake
# ac-import writes the package from the player's disc into OUT_DIR. Each run shows the game in real time in a window
# on SDL's offscreen video driver, through lavapipe's Vulkan, with SDL's dummy audio driver: nothing opens on the
# desktop and the game's sound is mixed but not heard. --user-dir keeps the settings under OUT_DIR/user, never in the
# player's directories.
#
# The settings round trip: the user directories start empty, and config/settings.toml is written first with a comment,
# the limiter at 120 Hz, the volume at 50 and a key no code registers, then the run of --ticks 1800 sets the limiter
# to 90 Hz for the session with --set. It must exit 0 with one play line and a window line with at least the limit's
# ticks (the main loop checks the limit once a presented frame) and frames presented; the game's sound must have mixed
# loud frames, the title's music, and the DSP rendered sounding frames, the boot jingle; and the window line must read
# the limiter at the override's 90 Hz and the gain of a volume of 50, 0.250, and end with the target the 1280x960
# window gives at the default render scale, the automatic one: 1280x960, twice the game's 640x480. The later runs
# that draw take --set sonar.video.render_scale=1, so lavapipe draws the game's own size.
#
# Unchanged settings: the round trip changes no setting, as an override is for the session and never a change, so
# afterwards the file must be byte for byte as written, its comment too: the limiter's 120, the volume's 50, and the
# key no code registers.
#
# The pipeline list: after the round trip, the GPU sink's list must exist in the user directories' cache,
# cache/pipelines.slpc, and the window line must count at least one list written: the run ends with _Exit, so the
# lists are those the sink wrote while it ran. The broken file's run then starts from it, so its window line must count
# pipelines warmed.
#
# A broken file: config/settings.toml is replaced by a line that does not parse, then a run of --ticks 120. Its stderr
# must name the file at line 1, its window line must read the default volume's gain, 1.000, and the target of render
# scale 1, 640x480, and the file must be byte for byte as written. Its exit status is not checked: 120 ticks reach no
# play.
#
# Bad assignments: with no settings file, a run of --ticks 60 with --set sonar.audio.volum=30, a key nothing
# registers, and one with --set sonar.audio.volume=500, a value its setting refuses, must each exit 2 with stderr naming
# the assignment; and --set, --user-dir and --resize without --window must each exit 2 with the usage.
#
# Bad resizes: --resize with the sizes 640, 640x, 0x480 and 640x480x2, a second at the first's tick, and a ninth must
# each exit 2 with the usage.
#
# A goldens display's arguments, with no --window: --display 854x480 without --goldens; --goldens with --display 854,
# no size; --goldens with --set sonar.video.widescreen=false and no --display, which binds no setting; and --goldens
# with --display 854x480 --set sonar.input.none=1, a key the display's video settings do not register, must each exit
# 2 with the usage. The goldens' scenes file is one line this script writes into OUT_DIR, and no run reaches a frame.
#
# The memory cards: with the user directories empty again, so no settings file, a run of --ticks 900, past play's
# first frame and so past CARDInit, must put card-a in slot A and none in B, its window line reading "cards card-a -",
# and the saves directory must hold card-a.slsave, the card's header save. A second run with --set
# kit.card.slot_b=card-b must read "cards card-a card-b" and the directory must hold card-b.slsave. If either run
# writes a settings file, it must hold no kit.card line: the defaults never changed, and an override is never saved.
# Their exit status is not checked.
#
# It prints every line it checks. When AC_DISC names no disc or there is no lavapipe it prints the line the test's
# SKIP_REGULAR_EXPRESSION matches and returns.
cmake_minimum_required(VERSION 3.25)

if("${AC_DISC}" STREQUAL "")
    message("ac_port_window: skipped, AC_DISC is not set")
    return()
endif()
if("${AC_LAVAPIPE_ICD}" STREQUAL "")
    message("ac_port_window: skipped, no lavapipe ICD for the offscreen driver's Vulkan surface")
    return()
endif()
foreach(variable IN ITEMS AC_IMPORT AC_PORT AC_VERSION OUT_DIR)
    if("${${variable}}" STREQUAL "")
        message(FATAL_ERROR "ac_port_window: ${variable} is not set")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUT_DIR}")
set(package "${OUT_DIR}/${AC_VERSION}.slpack")
execute_process(COMMAND "${AC_IMPORT}" "${AC_DISC}" "${package}" "${OUT_DIR}/cache"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ac_port_window: ac-import failed (${result}):\n${output}${errors}")
endif()
message("ac-import: ${output}")

set(user "${OUT_DIR}/user")
set(settings "${user}/config/settings.toml")

# Runs ac_port --window with the arguments given, setting result, output and errors, the play lines, the window line
# and its fields, and seconds, the run's time.
macro(run_window)
    string(TIMESTAMP started "%s")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy
                            "VK_DRIVER_FILES=${AC_LAVAPIPE_ICD}" "${AC_PORT}" --package "${package}" --window
                            --user-dir "${user}" ${ARGN}
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 600)
    string(TIMESTAMP ended "%s")
    math(EXPR seconds "${ended} - ${started}")
    string(REGEX MATCHALL "ac_port: play [^\n]*" plays "${output}")
    string(REGEX MATCH "ac_port: settings [^\n]*" settings_line "${output}")
    # CMake keeps nine groups, so only the fields read below are captured.
    string(CONCAT window_line "ac_port: window ticks ([0-9]+) frames ([0-9]+) blended [0-9]+ audio [0-9]+ "
           "loud ([0-9]+) dsp ([0-9]+) pacing [a-z]+ limit [0-9]+ gain [0-9.]+ pipelines [0-9]+ "
           "warmed ([0-9]+) lists ([0-9]+) specialized [0-9]+ specialized-warmed [0-9]+ unspecialized [0-9]+ "
           "cards ([^ \n]+) ([^ \n]+) draw-scale [0-9]+ draw-scale-changes [0-9]+ gpu-timed [0-9]+ gpu-over [0-9]+ "
           "target ([0-9]+x[0-9]+)")
    string(REGEX MATCH "${window_line}" line "${output}")
    set(ticks "${CMAKE_MATCH_1}")
    set(frames "${CMAKE_MATCH_2}")
    set(loud "${CMAKE_MATCH_3}")
    set(dsp "${CMAKE_MATCH_4}")
    set(warmed "${CMAKE_MATCH_5}")
    set(lists "${CMAKE_MATCH_6}")
    set(cards "${CMAKE_MATCH_7} ${CMAKE_MATCH_8}")
    set(target "${CMAKE_MATCH_9}")
    list(LENGTH plays count)
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

# The settings round trip.
set(limit 1800)
file(REMOVE_RECURSE "${user}")
string(CONCAT written "# The player's note: a run that changes no setting leaves it.\n"
       "sonar.video.pacing = \"limiter\"\nsonar.video.frame_limit = 120\n"
       "sonar.audio.volume = 50\nport.later.kept = 1\n")
file(WRITE "${settings}" "${written}")
file(SHA256 "${settings}" before)
run_window(--ticks ${limit} --set sonar.video.frame_limit=90)
message("round trip: exit ${result}, ${seconds} s:\n${settings_line}\n${plays}\n${line}")
if(NOT result EQUAL 0 OR NOT count EQUAL 1 OR line STREQUAL "")
    tail_of("${errors}" errors)
    message(FATAL_ERROR "ac_port_window: the run did not reach play's first frame and end at the limit:\n${errors}")
endif()
if(NOT settings_line STREQUAL "ac_port: settings ${settings}")
    message(FATAL_ERROR "ac_port_window: the settings line does not name ${settings}:\n${settings_line}")
endif()
if(ticks LESS limit OR NOT frames GREATER 0)
    message(FATAL_ERROR "ac_port_window: the window line has fewer ticks than the limit or no frame presented:\n${line}")
endif()
if(NOT loud GREATER 0)
    message(FATAL_ERROR "ac_port_window: the game's sound mixed no loud frame:\n${line}")
endif()
if(NOT dsp GREATER 0)
    message(FATAL_ERROR "ac_port_window: the DSP rendered no sounding frame, so the boot jingle is silent:\n${line}")
endif()
string(FIND "${line}" " pacing limiter limit 90 gain 0.250" at)
if(at EQUAL -1)
    message(FATAL_ERROR "ac_port_window: the window line does not read the limiter at 90 Hz and a gain of 0.250:\n${line}")
endif()
if(NOT target STREQUAL "1280x960")
    message(FATAL_ERROR "ac_port_window: the window line does not end with the automatic scale's target, 1280x960:\n"
                        "${line}")
endif()
string(FIND "${errors}" "ac_port: cannot save settings" at)
if(NOT at EQUAL -1)
    tail_of("${errors}" errors)
    message(FATAL_ERROR "ac_port_window: a save of the settings failed:\n${errors}")
endif()

# Unchanged settings.
file(SHA256 "${settings}" after)
file(READ "${settings}" kept_text)
message("unchanged settings: ${settings} sha256 ${before} before, ${after} after:\n${kept_text}")
if(NOT after STREQUAL before OR NOT kept_text STREQUAL written)
    message(FATAL_ERROR "ac_port_window: a run that changed no setting rewrote the settings file:\n${kept_text}")
endif()

# The pipeline list.
set(pipeline_list "${user}/cache/pipelines.slpc")
set(list_size "none")
if(EXISTS "${pipeline_list}")
    file(SIZE "${pipeline_list}" list_size)
endif()
message("pipeline list: ${pipeline_list} ${list_size} bytes, lists ${lists}")
if(list_size STREQUAL "none" OR NOT lists GREATER 0)
    message(FATAL_ERROR "ac_port_window: the run wrote no pipeline list to ${pipeline_list}:\n${line}")
endif()

# A broken file.
set(limit 120)
set(broken "sonar.audio.volume = fifty\n")
file(WRITE "${settings}" "${broken}")
file(SHA256 "${settings}" before)
run_window(--ticks ${limit} --set sonar.video.render_scale=1)
string(REGEX MATCHALL "ac_port: [^\n]*; it is left as it is" reported "${errors}")
message("broken file: exit ${result}, ${seconds} s:\n${settings_line}\n${reported}\n${line}")
string(FIND "${errors}" "ac_port: ${settings}:1:" at)
if(at EQUAL -1 OR reported STREQUAL "")
    tail_of("${errors}" errors)
    message(FATAL_ERROR "ac_port_window: stderr does not name ${settings} at line 1:\n${errors}")
endif()
string(FIND "${line}" " gain 1.000" at)
if(line STREQUAL "" OR at EQUAL -1)
    tail_of("${errors}" errors)
    message(FATAL_ERROR "ac_port_window: the window line does not read a gain of 1.000:\n${line}\n${errors}")
endif()
if(NOT target STREQUAL "640x480")
    message(FATAL_ERROR "ac_port_window: the window line does not end with render scale 1's target, 640x480:\n${line}")
endif()
message("pipeline list: warmed ${warmed}")
if(NOT warmed GREATER 0)
    message(FATAL_ERROR "ac_port_window: the second run warmed no pipeline from ${pipeline_list}:\n${line}")
endif()
file(SHA256 "${settings}" after)
file(READ "${settings}" kept_text)
message("broken file: ${settings} sha256 ${before} before, ${after} after")
if(NOT after STREQUAL before OR NOT kept_text STREQUAL broken)
    message(FATAL_ERROR "ac_port_window: the broken settings file was changed:\n${kept_text}")
endif()

# Bad assignments.
file(REMOVE "${settings}")
foreach(assignment IN ITEMS sonar.audio.volum=30 sonar.audio.volume=500)
    run_window(--ticks 60 --set ${assignment})
    string(REGEX MATCH "ac_port: --set [^\n]*" named "${errors}")
    message("bad assignment: exit ${result}: ${named}")
    if(NOT result EQUAL 2 OR NOT named MATCHES "^ac_port: --set ${assignment}: ")
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_window: --set ${assignment} did not exit 2 naming it:\n${errors}")
    endif()
endforeach()
foreach(option IN ITEMS "--set;sonar.audio.volume=30" "--user-dir;${user}" "--resize;10;640x480")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy
                            "VK_DRIVER_FILES=${AC_LAVAPIPE_ICD}" "${AC_PORT}" --package "${package}" --ticks 60
                            ${option}
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 60)
    list(GET option 0 name)
    string(FIND "${errors}" "usage: ac_port" at)
    message("without --window: ${name}: exit ${result}")
    if(NOT result EQUAL 2 OR at EQUAL -1)
        message(FATAL_ERROR "ac_port_window: ${name} without --window did not exit 2 with the usage:\n${errors}")
    endif()
endforeach()

# Bad resizes.
set(ninth "")
foreach(tick RANGE 1 9)
    list(APPEND ninth --resize ${tick} 640x480)
endforeach()
foreach(arguments IN ITEMS "--resize;10;640" "--resize;10;640x" "--resize;10;0x480" "--resize;10;640x480x2"
                           "--resize;20;640x480;--resize;20;800x600" "${ninth}")
    run_window(--ticks 60 ${arguments})
    string(FIND "${errors}" "usage: ac_port" at)
    message("bad resize: exit ${result}: ${arguments}")
    if(NOT result EQUAL 2 OR at EQUAL -1)
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_window: ${arguments} did not exit 2 with the usage:\n${errors}")
    endif()
endforeach()

# A goldens display's arguments.
set(scenes "${OUT_DIR}/display.scenes")
set(frames_dir "${OUT_DIR}/display")
file(WRITE "${scenes}" "logo_held menu_frame=64 fader=1\n")
file(REMOVE_RECURSE "${frames_dir}")
file(MAKE_DIRECTORY "${frames_dir}")
foreach(arguments IN ITEMS "--ticks;60;--display;854x480" "--goldens;${scenes};${frames_dir};--display;854"
                           "--goldens;${scenes};${frames_dir};--set;sonar.video.widescreen=false"
                           "--goldens;${scenes};${frames_dir};--display;854x480;--set;sonar.input.none=1")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy
                            "VK_DRIVER_FILES=${AC_LAVAPIPE_ICD}" "${AC_PORT}" --package "${package}" ${arguments}
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 60)
    string(FIND "${errors}" "usage: ac_port" at)
    string(REPLACE "${OUT_DIR}/" "" named "${arguments}")
    string(REPLACE ";" " " named "${named}")
    message("goldens display: exit ${result}: ${named}")
    if(NOT result EQUAL 2 OR at EQUAL -1)
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_window: ${named} did not exit 2 with the usage:\n${errors}")
    endif()
endforeach()

# The memory cards.
file(REMOVE_RECURSE "${user}")
set(saves "${user}/saves")
foreach(run IN ITEMS "card-a -" "card-a card-b")
    if(run STREQUAL "card-a -")
        run_window(--ticks 900 --set sonar.video.render_scale=1)
        set(card "card-a")
    else()
        run_window(--ticks 900 --set sonar.video.render_scale=1 --set kit.card.slot_b=card-b)
        set(card "card-b")
    endif()
    file(GLOB held RELATIVE "${saves}" "${saves}/*.slsave")
    message("memory cards: exit ${result}, ${seconds} s: cards ${cards}; ${saves} holds ${held}")
    if(line STREQUAL "" OR NOT cards STREQUAL run)
        tail_of("${errors}" errors)
        message(FATAL_ERROR "ac_port_window: the window line does not read cards ${run}:\n${line}\n${errors}")
    endif()
    if(NOT EXISTS "${saves}/${card}.slsave")
        message(FATAL_ERROR "ac_port_window: ${saves} holds no ${card}.slsave, the card's header save")
    endif()
    if(EXISTS "${settings}")
        file(READ "${settings}" written_text)
        message("memory cards: the run wrote ${settings}:\n${written_text}")
        string(FIND "${written_text}" "kit.card" at)
        if(NOT at EQUAL -1)
            message(FATAL_ERROR "ac_port_window: the settings file holds a kit.card line:\n${written_text}")
        endif()
    endif()
endforeach()
