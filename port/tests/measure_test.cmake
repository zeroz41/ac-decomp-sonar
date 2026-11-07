# The port's measured runs (sonarlib docs/roadmap.md, M6 step 7b; port/NOTES.md, Measured runs):
#   cmake -DAC_IMPORT=<ac-import> -DAC_PORT=<ac_port> -DAC_DISC=<disc> -DAC_VERSION=<version> -DOUT_DIR=<dir>
#         -DAC_LAVAPIPE_ICD=<lavapipe's ICD> -P measure_test.cmake
# ac-import writes the package from the player's disc into OUT_DIR. Then ac_port runs once as it is and once with
# --reference, the console's emu64 drawing instead of the kit, to play's first frame: the reference's game is the
# play line's, with no palette overflowed. Then --measure --ticks 1200 runs twice with the kit and twice with
# --reference, on the GPU sink with no window through SDL's offscreen video driver and lavapipe's Vulkan. Each exits 0
# with a measure line and a measured line, the same in both runs once the nine times are taken out, with frames, ticks,
# draws and calls, and nothing invalid, overflowed or refused. The kit's and the reference's run the same game for the
# same frames. It prints every line. When AC_DISC names no disc or there is no lavapipe it prints the line the test's
# SKIP_REGULAR_EXPRESSION matches and returns.
cmake_minimum_required(VERSION 3.25)

if("${AC_DISC}" STREQUAL "")
    message("ac_port_measure: skipped, AC_DISC is not set")
    return()
endif()
if("${AC_LAVAPIPE_ICD}" STREQUAL "")
    message("ac_port_measure: skipped, no lavapipe ICD for the offscreen driver's Vulkan")
    return()
endif()
foreach(variable IN ITEMS AC_IMPORT AC_PORT AC_VERSION OUT_DIR)
    if("${${variable}}" STREQUAL "")
        message(FATAL_ERROR "ac_port_measure: ${variable} is not set")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUT_DIR}")
set(package "${OUT_DIR}/${AC_VERSION}.slpack")
execute_process(COMMAND "${AC_IMPORT}" "${AC_DISC}" "${package}" "${OUT_DIR}/cache"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ac_port_measure: ac-import failed (${result}):\n${output}${errors}")
endif()
message("ac-import: ${output}")

set(count "[0-9]+")
set(report_pattern " tick ${count} retrace ${count} gx ${count} dvd ${count} ${count} hash [0-9a-f]+")
string(APPEND report_pattern " instances ${count} compiles ${count} holes ${count}/${count} cpu ${count}")
string(APPEND report_pattern " refused ${count} views ${count} invalid ${count} problems ${count} overflows ${count}")
string(APPEND report_pattern " unowned ${count} gx-errors ${count} unmapped ${count} outside ${count} nested ${count}")
string(APPEND report_pattern " refused-frames ${count} audio ${count} loud ${count} dsp ${count}")
string(APPEND report_pattern " sound [0-9a-f]+( palette-overflows ${count})?")
set(time "${count} ${count} ${count}")
set(measure_pattern "ac_port: measure (kit|reference) frames ${count} ticks ${count} game ${time}")
string(APPEND measure_pattern " prepare ${time} present ${time} draws ${count} calls ${count} pipelines ${count}")
string(APPEND measure_pattern " buffers ${count} textures ${count} storage ${count} pushes ${count}")

# Runs ac_port with the options, and fails unless it exits 0 with one line matching each pattern, in order, into the
# list out.
function(run name out)
    cmake_parse_arguments(PARSE_ARGV 2 arg "MEASURE" "" "OPTIONS;PATTERNS")
    set(environment "")
    if(arg_MEASURE)
        set(environment SDL_VIDEO_DRIVER=offscreen "VK_DRIVER_FILES=${AC_LAVAPIPE_ICD}")
    endif()
    string(TIMESTAMP started "%s")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${environment} "${AC_PORT}" --package "${package}"
                            ${arg_OPTIONS}
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 1200)
    string(TIMESTAMP ended "%s")
    math(EXPR seconds "${ended} - ${started}")
    set(lines "")
    set(missing FALSE)
    foreach(pattern IN LISTS arg_PATTERNS)
        string(REGEX MATCHALL "${pattern}" matches "${output}")
        list(LENGTH matches matched)
        if(NOT matched EQUAL 1)
            set(missing TRUE)
        endif()
        list(APPEND lines "${matches}")
    endforeach()
    list(JOIN lines "\n" printed)
    message("${name}: exit ${result}, ${seconds} s:\n${printed}")
    if(NOT result EQUAL 0 OR missing)
        string(LENGTH "${errors}" length)
        if(length GREATER 4000)
            math(EXPR from "${length} - 4000")
            string(SUBSTRING "${errors}" ${from} -1 errors)
        endif()
        message(FATAL_ERROR "ac_port_measure: ${name} did not exit 0 with one line of each:\n${output}${errors}")
    endif()
    set(${out} "${lines}" PARENT_SCOPE)
endfunction()

# The fields the kit has no part in: the game's ticks, its retraces, DVD reads and sound.
function(game_fields line out)
    string(REGEX MATCH " tick ([0-9]+) retrace ([0-9]+) gx [0-9]+ dvd ([0-9]+ [0-9]+) " ignored "${line}")
    set(fields "tick ${CMAKE_MATCH_1} retrace ${CMAKE_MATCH_2} dvd ${CMAKE_MATCH_3}")
    string(REGEX MATCH " audio ([0-9]+) loud ([0-9]+) dsp ([0-9]+) sound ([0-9a-f]+)" ignored "${line}")
    string(APPEND fields " audio ${CMAKE_MATCH_1} loud ${CMAKE_MATCH_2} dsp ${CMAKE_MATCH_3} sound ${CMAKE_MATCH_4}")
    set(${out} "${fields}" PARENT_SCOPE)
endfunction()

# Fails unless the report line counts nothing invalid, overflowed or refused, and none of the GX glue's errors.
function(check_report name line)
    string(REGEX MATCH " refused ([0-9]+) views ([0-9]+) invalid ([0-9]+) problems ([0-9]+) overflows ([0-9]+) "
           ignored "${line}")
    set(values "${CMAKE_MATCH_1};${CMAKE_MATCH_2};${CMAKE_MATCH_3};${CMAKE_MATCH_4};${CMAKE_MATCH_5}")
    set(names "refused;views;invalid;problems;overflows")
    string(REGEX MATCH
           " gx-errors ([0-9]+) unmapped ([0-9]+) outside ([0-9]+) nested ([0-9]+) refused-frames ([0-9]+) "
           ignored "${line}")
    list(APPEND values "${CMAKE_MATCH_1};${CMAKE_MATCH_2};${CMAKE_MATCH_3};${CMAKE_MATCH_4};${CMAKE_MATCH_5}")
    list(APPEND names "gx-errors;unmapped;outside;nested;refused-frames")
    if(line MATCHES " palette-overflows ([0-9]+)")
        list(APPEND values "${CMAKE_MATCH_1}")
        list(APPEND names "palette-overflows")
    endif()
    foreach(value field IN ZIP_LISTS values names)
        if(NOT value EQUAL 0)
            message(FATAL_ERROR "ac_port_measure: ${name}'s ${field} is not 0:\n${line}")
        endif()
    endforeach()
endfunction()

# Play's first frame, drawn by the kit and by the reference.
run(play play_lines PATTERNS "ac_port: play${report_pattern}")
run(reference reference_lines OPTIONS --reference PATTERNS "ac_port: reference${report_pattern}")
game_fields("${play_lines}" play_game)
game_fields("${reference_lines}" reference_game)
if(NOT reference_game STREQUAL play_game)
    message(FATAL_ERROR "ac_port_measure: the reference's game differs from play's:\n${play_lines}\n${reference_lines}")
endif()
if(NOT reference_lines MATCHES " palette-overflows 0$")
    message(FATAL_ERROR "ac_port_measure: the reference overflowed a palette:\n${reference_lines}")
endif()

# The measured runs, twice each.
foreach(drawer IN ITEMS kit reference)
    set(options --measure --ticks 1200)
    if(drawer STREQUAL "reference")
        list(APPEND options --reference)
    endif()
    set(untimed "")
    foreach(attempt IN ITEMS 1 2)
        run("${drawer} ${attempt}" lines MEASURE OPTIONS ${options}
            PATTERNS "${measure_pattern}" "ac_port: measured${report_pattern}")
        list(GET lines 0 measure_line)
        list(GET lines 1 measured_line)
        if(NOT measure_line MATCHES "^ac_port: measure ${drawer} ")
            message(FATAL_ERROR "ac_port_measure: the measure line is not the ${drawer}'s:\n${measure_line}")
        endif()
        string(REGEX MATCH " frames ([0-9]+) ticks ([0-9]+) " ignored "${measure_line}")
        set(frames "${CMAKE_MATCH_1}")
        set(ticks "${CMAKE_MATCH_2}")
        string(REGEX MATCH " draws ([0-9]+) calls ([0-9]+) " ignored "${measure_line}")
        if(NOT frames GREATER 0 OR NOT ticks GREATER 0 OR NOT CMAKE_MATCH_1 GREATER 0 OR NOT CMAKE_MATCH_2 GREATER 0)
            message(FATAL_ERROR "ac_port_measure: the ${drawer}'s run measured no frame or no draw:\n${measure_line}")
        endif()
        check_report("the ${drawer}'s measured run" "${measured_line}")
        string(REGEX REPLACE " (game|prepare|present) [0-9]+ [0-9]+ [0-9]+" " \\1" measure_line "${measure_line}")
        list(APPEND untimed "${measure_line}\n${measured_line}")
    endforeach()
    list(GET untimed 0 first)
    list(GET untimed 1 second)
    if(NOT first STREQUAL second)
        message(FATAL_ERROR "ac_port_measure: the ${drawer}'s two runs differ:\n${first}\n${second}")
    endif()
    set(${drawer}_frames "frames ${frames} ticks ${ticks}")
    game_fields("${measured_line}" ${drawer}_game)
endforeach()
if(NOT kit_frames STREQUAL reference_frames OR NOT kit_game STREQUAL reference_game)
    message(FATAL_ERROR "ac_port_measure: the kit's and the reference's runs differ:\n"
                        "${kit_frames}, ${kit_game}\n${reference_frames}, ${reference_game}")
endif()
