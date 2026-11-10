# The port's goldens against Dolphin (sonarlib docs/roadmap.md, M6 step 6n; port/NOTES.md, Goldens):
#   cmake -DAC_IMPORT=<ac-import> -DAC_PORT=<ac_port> -DSL_CAPTURE=<sl-capture> -DAC_DOLPHIN=<dolphin-emu-nogui>
#         -DAC_DISC=<disc> -DAC_VERSION=<version> -DSCENES=<goldens.scenes> -DOUT_DIR=<dir>
#         -DAC_LAVAPIPE_ICD=<lavapipe's ICD> -P goldens_test.cmake
# sl-capture runs Dolphin on the player's disc with no input and caches its frames for the scenes in OUT_DIR/dolphin,
# given relative to OUT_DIR, as MemoryWatcher's socket path must fit a Unix socket's 107 bytes, and keyed by Dolphin's
# version, the disc and the settings, with the keys' locations at the decomp's symbols. Then
# ac-import writes the package, ac_port --goldens writes the port's frames for the same scenes, on the GPU sink through
# SDL's offscreen video driver and lavapipe's Vulkan, with nothing opened on the desktop, seeded with Dolphin's qrand
# seed, and sl-capture compares them. Then the same with --reference, the console's emu64 drawing instead of the kit
# (M6 step 7b), into port-reference. Then the kit's frames three times more on a display wider than the game's (M6
# step 9c2), a virtual window --display makes: at 16:9 into port-16x9 and at 21:9 into port-21x9, where each frame is
# wider by the columns the display adds and its middle is compared with Dolphin's frame, and at 16:9 with widescreen
# off into port-16x9-off, where each frame must be the first run's byte for byte. It prints every line. When AC_DISC
# names no disc, AC_DOLPHIN no Dolphin, there is no lavapipe or no sl-capture, or the disc is another version than
# GAFE01_00, whose addresses these are, it prints the line the test's SKIP_REGULAR_EXPRESSION matches and returns.
cmake_minimum_required(VERSION 3.25)

if("${AC_DISC}" STREQUAL "")
    message("ac_port_goldens: skipped, AC_DISC is not set")
    return()
endif()
if("${AC_DOLPHIN}" STREQUAL "" OR NOT EXISTS "${AC_DOLPHIN}")
    message("ac_port_goldens: skipped, AC_DOLPHIN names no dolphin-emu-nogui")
    return()
endif()
if("${AC_LAVAPIPE_ICD}" STREQUAL "")
    message("ac_port_goldens: skipped, no lavapipe ICD for the offscreen driver's Vulkan")
    return()
endif()
if("${SL_CAPTURE}" STREQUAL "")
    message("ac_port_goldens: skipped, sonarlib built no sl-capture (it needs SDL3 and a POSIX host)")
    return()
endif()
if(NOT AC_VERSION STREQUAL "GAFE01_00")
    message("ac_port_goldens: skipped, the keys' addresses are GAFE01_00's, not ${AC_VERSION}'s")
    return()
endif()
foreach(variable IN ITEMS AC_IMPORT AC_PORT SCENES OUT_DIR)
    if("${${variable}}" STREQUAL "")
        message(FATAL_ERROR "ac_port_goldens: ${variable} is not set")
    endif()
endforeach()

# An extracted disc boots its main.dol; an image boots itself.
if(IS_DIRECTORY "${AC_DISC}")
    set(boot "${AC_DISC}/sys/main.dol")
else()
    set(boot "${AC_DISC}")
endif()

# The keys at the decomp's symbols: initial_menu's frame_count; jc_fader's mStatus and its ticks target and run; and
# through the OS module list's first module, foresta.rel, its .bss's game_class_p, the game's frame_counter and exec,
# less foresta's text section's entry. The qrand seed, __qrand_idum, is watched for watch.tsv.
file(MAKE_DIRECTORY "${OUT_DIR}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "VK_DRIVER_FILES=${AC_LAVAPIPE_ICD}"
                        "${SL_CAPTURE}" dolphin --dolphin "${AC_DOLPHIN}" --boot "${boot}" --scenes "${SCENES}"
                        --cache dolphin --frame 802083DC --lag 2 --check-counter menu_frame
                        --watch "menu_frame=800E2698" --watch "fader=800E2608 4" --watch "fader_time=800E2608 8"
                        --watch "game_frame=800030C8 10 30 98A24 A0"
                        --watch "play=800030C8 10 30 98A24 4-800030C8 10 8" --watch "seed=80217DE8"
                        --game-ini GAFE01 Video_Settings.SafeTextureCacheColorSamples=0 --timeout 600
                WORKING_DIRECTORY "${OUT_DIR}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 900)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ac_port_goldens: sl-capture dolphin failed (${result}):\n${output}${errors}")
endif()
string(STRIP "${output}" reference)
string(REGEX REPLACE ".*\n" "" reference "${reference}") # the cache directory, its last line
set(reference "${OUT_DIR}/${reference}")
message("sl-capture dolphin: ${reference}")

# The seed: __qrand_idum's first value after its start, init_rnd's sqrand.
file(STRINGS "${reference}/watch.tsv" seeds REGEX "^[0-9]+[ \t]seed[ \t]")
list(LENGTH seeds count)
if(count LESS 2)
    message(FATAL_ERROR "ac_port_goldens: Dolphin's watch.tsv holds no seed after the first")
endif()
list(GET seeds 1 seed)
string(REGEX REPLACE "^.*[ \t]" "" seed "${seed}")

set(package "${OUT_DIR}/${AC_VERSION}.slpack")
execute_process(COMMAND "${AC_IMPORT}" "${AC_DISC}" "${package}" "${OUT_DIR}/cache"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ac_port_goldens: ac-import failed (${result}):\n${output}${errors}")
endif()

# One run: ac_port --goldens with the options into OUT_DIR/<name>, and sl-capture compare of its frames with Dolphin's,
# with the compare's options.
function(goldens_run name port_options compare_options)
    file(REMOVE_RECURSE "${OUT_DIR}/${name}")
    file(MAKE_DIRECTORY "${OUT_DIR}/${name}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env SDL_VIDEO_DRIVER=offscreen "VK_DRIVER_FILES=${AC_LAVAPIPE_ICD}"
                            "${AC_PORT}" --package "${package}" ${port_options} --goldens "${SCENES}"
                            "${OUT_DIR}/${name}" --qrand-seed "${seed}"
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 600)
    string(REGEX MATCH "ac_port: goldens [^\n]*" line "${output}")
    string(REPLACE ";" " " shown_options "${port_options}")
    message("ac_port ${shown_options}: seed ${seed}, exit ${result}\n${line}")
    if(NOT result EQUAL 0 OR line STREQUAL "")
        string(LENGTH "${errors}" length)
        if(length GREATER 4000)
            math(EXPR from "${length} - 4000")
            string(SUBSTRING "${errors}" ${from} -1 errors)
        endif()
        message(FATAL_ERROR "ac_port_goldens: the ${name} run did not write every scene:\n${output}${errors}")
    endif()

    execute_process(COMMAND "${SL_CAPTURE}" compare --reference "${reference}" --port "${OUT_DIR}/${name}"
                            --scenes "${SCENES}" ${compare_options}
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
    message("${output}${errors}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
                "ac_port_goldens: the ${name} frames differ from Dolphin's (pictures in ${OUT_DIR}/${name})")
    endif()
endfunction()

# The scenes a run wrote, from its scenes.tsv, into the caller's `scenes`.
function(goldens_scenes name)
    file(STRINGS "${OUT_DIR}/${name}/scenes.tsv" lines)
    set(scenes "")
    foreach(line IN LISTS lines)
        string(REGEX REPLACE "\t.*$" "" scene "${line}")
        list(APPEND scenes "${scene}")
    endforeach()
    if(scenes STREQUAL "")
        message(FATAL_ERROR "ac_port_goldens: the ${name} run's scenes.tsv names no scene")
    endif()
    set(scenes "${scenes}" PARENT_SCOPE)
endfunction()

# Every frame of a run has its kind's size: a play scene's `play`, as "<width> <height>", and the logo's `logo`.
function(goldens_sizes name play logo)
    goldens_scenes(${name})
    foreach(scene IN LISTS scenes)
        if(scene MATCHES "^play_")
            set(size "${play}")
        else()
            set(size "${logo}")
        endif()
        string(HEX "P6\n${size}\n255\n" expected)
        string(LENGTH "${expected}" length)
        math(EXPR bytes "${length} / 2")
        file(READ "${OUT_DIR}/${name}/${scene}.ppm" header LIMIT ${bytes} HEX)
        if(NOT header STREQUAL expected)
            message(FATAL_ERROR "ac_port_goldens: ${name}/${scene}.ppm is not ${size}")
        endif()
    endforeach()
    list(LENGTH scenes count)
    message("ac_port_goldens: ${name}: ${count} frames, play ${play}, logo ${logo}")
endfunction()

# Every frame of a run is another run's, byte for byte.
function(goldens_same name other)
    goldens_scenes(${name})
    foreach(scene IN LISTS scenes)
        execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${OUT_DIR}/${name}/${scene}.ppm"
                                "${OUT_DIR}/${other}/${scene}.ppm"
                        RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "ac_port_goldens: ${name}/${scene}.ppm is not ${other}'s, byte for byte")
        endif()
    endforeach()
    list(LENGTH scenes count)
    message("ac_port_goldens: ${name}: ${count} frames, each ${other}'s byte for byte")
endfunction()

# The kit's frames into port, and the reference's, drawn by the console's emu64, into port-reference.
goldens_run(port "" "")
goldens_run(port-reference "--reference" "")

# The kit's frames on a wider display. The game's frame is 640 wide, so a 16:9 display of its height adds 214 columns
# and a 21:9 one 480, half on each side, and the VI's crop of a play frame, 598 x 464, is taken from each edge of the
# display. The frame's middle is the 1x frame, compared at its tolerance; nothing here shows what the sides should
# hold (sonarlib docs/validation.md, Port goldens).
goldens_run(port-16x9 "--display;854x480" "--added;214")
goldens_sizes(port-16x9 "812 464" "854 480")
goldens_run(port-21x9 "--display;1120x480" "--added;480")
goldens_sizes(port-21x9 "1078 464" "1120 480")

# Widescreen off in a 16:9 display, the setting's off switch: the frames the first run wrote.
goldens_run(port-16x9-off "--display;854x480;--set;sonar.video.widescreen=false" "")
goldens_sizes(port-16x9-off "598 464" "640 480")
goldens_same(port-16x9-off port)
