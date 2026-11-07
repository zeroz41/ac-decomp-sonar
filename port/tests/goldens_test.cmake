# The port's goldens against Dolphin (sonarlib docs/roadmap.md, M6 step 6n; port/NOTES.md, Goldens):
#   cmake -DAC_IMPORT=<ac-import> -DAC_PORT=<ac_port> -DSL_CAPTURE=<sl-capture> -DAC_DOLPHIN=<dolphin-emu-nogui>
#         -DAC_DISC=<disc> -DAC_VERSION=<version> -DSCENES=<goldens.scenes> -DOUT_DIR=<dir>
#         -DAC_LAVAPIPE_ICD=<lavapipe's ICD> -P goldens_test.cmake
# sl-capture runs Dolphin on the player's disc with no input and caches its frames for the scenes in OUT_DIR/dolphin,
# given relative to OUT_DIR, as MemoryWatcher's socket path must fit a Unix socket's 107 bytes, and keyed by Dolphin's
# version, the disc and the settings, with the keys' locations at the decomp's symbols. Then
# ac-import writes the package, ac_port --goldens writes the port's frames for the same scenes, on the GPU sink with no
# window through SDL's offscreen video driver and lavapipe's Vulkan, seeded with Dolphin's qrand seed, and sl-capture
# compares them. It prints every line. When AC_DISC names no disc, AC_DOLPHIN no Dolphin, there is no lavapipe or no
# sl-capture, or the disc is another version than GAFE01_00, whose addresses these are, it prints the line the test's
# SKIP_REGULAR_EXPRESSION matches and returns.
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

file(REMOVE_RECURSE "${OUT_DIR}/port")
file(MAKE_DIRECTORY "${OUT_DIR}/port")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env SDL_VIDEO_DRIVER=offscreen "VK_DRIVER_FILES=${AC_LAVAPIPE_ICD}"
                        "${AC_PORT}" --package "${package}" --goldens "${SCENES}" "${OUT_DIR}/port"
                        --qrand-seed "${seed}"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 600)
string(REGEX MATCH "ac_port: goldens [^\n]*" line "${output}")
message("ac_port: seed ${seed}, exit ${result}\n${line}")
if(NOT result EQUAL 0 OR line STREQUAL "")
    string(LENGTH "${errors}" length)
    if(length GREATER 4000)
        math(EXPR from "${length} - 4000")
        string(SUBSTRING "${errors}" ${from} -1 errors)
    endif()
    message(FATAL_ERROR "ac_port_goldens: the port did not write every scene:\n${output}${errors}")
endif()

execute_process(COMMAND "${SL_CAPTURE}" compare --reference "${reference}" --port "${OUT_DIR}/port" --scenes "${SCENES}"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
message("${output}${errors}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ac_port_goldens: the port's frames differ from Dolphin's (pictures in ${OUT_DIR}/port)")
endif()
