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
# off into port-16x9-off, where each frame must be the first run's byte for byte. Then at 21:9 with the UI scale at 50
# (M6 step 9d2) into port-21x9-ui50, compared with no Dolphin frame, which shows no scaled 2D: its frames have
# port-21x9's sizes, and the logo held and each play scene must differ from port-21x9's, as each holds 2D the
# setting reaches. Then the picture's settings in the 16:9 display (M6 step 9e1), of which a frame shows nothing: with a
# display each scene has a window picture too, <scene>.window.ppm, the display's whole image under the run's
# settings, 854 x 480 at 16:9. With SMAA, FSR 1 and RCAS into port-16x9-post, each frame must be port-16x9's byte for
# byte and the window pictures of the logo held and of each play scene must differ from port-16x9's. With
# supersampling at 2 into port-16x9-ss2, each frame must be twice port-16x9's size and the same window pictures must
# differ. Each of the two then runs again, and every frame and window picture must be the first run's byte for byte.
# Then multisampling at 4 in the 16:9 display (M6 step 9e2) into port-16x9-msaa4, which changes the frames themselves:
# they and the window pictures have port-16x9's sizes, each play scene's frame must differ from port-16x9's, and the
# run again must write the first run's frames and window pictures byte for byte.
# It prints every line. When AC_DISC names no disc,
# AC_DOLPHIN no Dolphin, there is no lavapipe or no sl-capture, or the disc is another version than GAFE01_00, whose
# addresses these are, it prints the line the test's SKIP_REGULAR_EXPRESSION matches and returns.
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

# One run's frames: ac_port --goldens with the options into OUT_DIR/<name>.
function(goldens_write name port_options)
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
endfunction()

# One run: its frames, and sl-capture compare of them with Dolphin's, with the compare's options.
function(goldens_run name port_options compare_options)
    goldens_write("${name}" "${port_options}")
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

# Whether a file of a run, a scene's frame or its window picture, is another run's, byte for byte, into the caller's
# `same`.
function(goldens_same_file name other file)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${OUT_DIR}/${name}/${file}"
                            "${OUT_DIR}/${other}/${file}"
                    RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
    if(result EQUAL 0)
        set(same TRUE PARENT_SCOPE)
    else()
        set(same FALSE PARENT_SCOPE)
    endif()
endfunction()

# Whether a scene's frame in a run is another run's, byte for byte, into the caller's `same`.
function(goldens_same_frame name other scene)
    goldens_same_file(${name} ${other} "${scene}.ppm")
    set(same ${same} PARENT_SCOPE)
endfunction()

# Every frame of a run is another run's, byte for byte.
function(goldens_same name other)
    goldens_scenes(${name})
    foreach(scene IN LISTS scenes)
        goldens_same_frame(${name} ${other} ${scene})
        if(NOT same)
            message(FATAL_ERROR "ac_port_goldens: ${name}/${scene}.ppm is not ${other}'s, byte for byte")
        endif()
    endforeach()
    list(LENGTH scenes count)
    message("ac_port_goldens: ${name}: ${count} frames, each ${other}'s byte for byte")
endfunction()

# A run at a UI scale against the same run at the game's size: the logo held and each play scene differ, as each holds
# 2D. No scene here is of the world alone: the play scenes are of the title's demo, where the title's letters wait at
# the frame's right edge (port/NOTES.md, What a wider window shows).
function(goldens_scaled name other)
    goldens_scenes(${name})
    set(count 0)
    foreach(scene IN LISTS scenes)
        if(scene MATCHES "^play_" OR scene STREQUAL "logo_held")
            goldens_same_frame(${name} ${other} ${scene})
            if(same)
                message(FATAL_ERROR
                        "ac_port_goldens: ${name}/${scene}.ppm is ${other}'s: the UI scale did not reach it")
            endif()
            math(EXPR count "${count} + 1")
        endif()
    endforeach()
    if(count LESS 2 OR NOT "logo_held" IN_LIST scenes)
        message(FATAL_ERROR "ac_port_goldens: the ${name} run wrote no play scene or no logo_held")
    endif()
    message("ac_port_goldens: ${name}: the logo held and each play scene, ${count} frames, differ from ${other}'s")
endfunction()

# Every scene of a run in a display has a window picture of the display's size, as "<width> <height>": the window's
# whole image under the run's settings (sonarlib's goldens.h).
function(goldens_window_sizes name size)
    goldens_scenes(${name})
    string(HEX "P6\n${size}\n255\n" expected)
    string(LENGTH "${expected}" length)
    math(EXPR bytes "${length} / 2")
    foreach(scene IN LISTS scenes)
        set(header "")
        if(EXISTS "${OUT_DIR}/${name}/${scene}.window.ppm")
            file(READ "${OUT_DIR}/${name}/${scene}.window.ppm" header LIMIT ${bytes} HEX)
        endif()
        if(NOT header STREQUAL expected)
            message(FATAL_ERROR "ac_port_goldens: ${name}/${scene}.window.ppm is not a window picture of ${size}")
        endif()
    endforeach()
    list(LENGTH scenes count)
    message("ac_port_goldens: ${name}: ${count} window pictures, each ${size}")
endfunction()

# A run's window pictures against another run's in the same display, both checked by goldens_window_sizes: the logo
# held and each play scene differ, as a setting of the picture reaches all a window shows.
function(goldens_windows_differ name other)
    goldens_scenes(${name})
    set(count 0)
    foreach(scene IN LISTS scenes)
        if(scene MATCHES "^play_" OR scene STREQUAL "logo_held")
            goldens_same_file(${name} ${other} "${scene}.window.ppm")
            if(same)
                message(FATAL_ERROR "ac_port_goldens: ${name}/${scene}.window.ppm is ${other}'s: the picture's "
                                    "settings did not reach the window")
            endif()
            math(EXPR count "${count} + 1")
        endif()
    endforeach()
    if(count LESS 2 OR NOT "logo_held" IN_LIST scenes)
        message(FATAL_ERROR "ac_port_goldens: the ${name} run wrote no play scene or no logo_held")
    endif()
    message("ac_port_goldens: ${name}: the window pictures of the logo held and each play scene, ${count}, differ "
            "from ${other}'s")
endfunction()

# A multisampled run against the same run at one sample, both checked by goldens_sizes: each play scene's frame
# differs, as MSAA changes the main target itself, at the edges of the shapes it holds.
function(goldens_resampled name other)
    goldens_scenes(${name})
    set(count 0)
    foreach(scene IN LISTS scenes)
        if(scene MATCHES "^play_")
            goldens_same_frame(${name} ${other} ${scene})
            if(same)
                message(FATAL_ERROR "ac_port_goldens: ${name}/${scene}.ppm is ${other}'s: the samples did not reach it")
            endif()
            math(EXPR count "${count} + 1")
        endif()
    endforeach()
    if(count LESS 1)
        message(FATAL_ERROR "ac_port_goldens: the ${name} run wrote no play scene")
    endif()
    message("ac_port_goldens: ${name}: each play scene, ${count} frames, differs from ${other}'s")
endfunction()

# A run a second time, into <name>-again: its scenes, and every frame and window picture, are the first run's, byte for
# byte.
function(goldens_again name port_options)
    goldens_write(${name}-again "${port_options}")
    goldens_scenes(${name})
    set(first "${scenes}")
    goldens_scenes(${name}-again)
    if(NOT scenes STREQUAL first)
        message(FATAL_ERROR "ac_port_goldens: ${name}'s second run wrote other scenes than its first")
    endif()
    foreach(scene IN LISTS scenes)
        foreach(file IN ITEMS "${scene}.ppm" "${scene}.window.ppm")
            goldens_same_file(${name}-again ${name} "${file}")
            if(NOT same)
                message(FATAL_ERROR "ac_port_goldens: ${name}-again/${file} is not the first run's, byte for byte")
            endif()
        endforeach()
    endforeach()
    list(LENGTH scenes count)
    message("ac_port_goldens: ${name}: run again, ${count} frames and ${count} window pictures, each the first "
            "run's byte for byte")
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
goldens_window_sizes(port-16x9 "854 480")
goldens_run(port-21x9 "--display;1120x480" "--added;480")
goldens_sizes(port-21x9 "1078 464" "1120 480")

# Widescreen off in a 16:9 display, the setting's off switch: the frames the first run wrote.
goldens_run(port-16x9-off "--display;854x480;--set;sonar.video.widescreen=false" "")
goldens_sizes(port-16x9-off "598 464" "640 480")
goldens_same(port-16x9-off port)

# The UI scale at 50 in the 21:9 display (M6 step 9d2). No Dolphin frame shows a scaled 2D, so the run is compared
# with the 21:9 run alone: its frames keep that run's sizes, and the setting reaches their 2D.
goldens_write(port-21x9-ui50 "--display;1120x480;--set;sonar.video.ui_scale=50")
goldens_sizes(port-21x9-ui50 "1078 464" "1120 480")
goldens_scaled(port-21x9-ui50 port-21x9)

# The picture's settings in the 16:9 display (M6 step 9e1). The post chain changes only what a window shows, so with
# SMAA, FSR 1 and RCAS the frames are the 16:9 run's and the window pictures are not. No Dolphin frame shows a filter.
set(post "--display;854x480;--set;sonar.video.antialiasing=smaa;--set;sonar.video.upscale=fsr1")
list(APPEND post "--set" "sonar.video.sharpen=rcas")
goldens_write(port-16x9-post "${post}")
goldens_sizes(port-16x9-post "812 464" "854 480")
goldens_same(port-16x9-post port-16x9)
goldens_window_sizes(port-16x9-post "854 480")
goldens_windows_differ(port-16x9-post port-16x9)
goldens_again(port-16x9-post "${post}")

# Supersampling at 2: the frames at twice the 16:9 run's size a side, the scale of 2 the factor asks for in a display
# of the game's height, and the window pictures, at the display's size, averaged from them.
set(supersampled "--display;854x480;--set;sonar.video.supersampling=2")
goldens_write(port-16x9-ss2 "${supersampled}")
goldens_sizes(port-16x9-ss2 "1624 928" "1708 960")
goldens_window_sizes(port-16x9-ss2 "854 480")
goldens_windows_differ(port-16x9-ss2 port-16x9)
goldens_again(port-16x9-ss2 "${supersampled}")

# Multisampling at 4 (M6 step 9e2). The port starts its sink at one sample, so the run changes the count at its first
# present, with the pipelines made by then. MSAA is the main target's: the frames keep the 16:9 run's sizes and each
# play scene's differs from that run's, where a filter changes the window pictures alone. No Dolphin frame shows it.
set(multisampled "--display;854x480;--set;sonar.video.msaa=4x")
goldens_write(port-16x9-msaa4 "${multisampled}")
goldens_sizes(port-16x9-msaa4 "812 464" "854 480")
goldens_window_sizes(port-16x9-msaa4 "854 480")
goldens_resampled(port-16x9-msaa4 port-16x9)
goldens_again(port-16x9-msaa4 "${multisampled}")
