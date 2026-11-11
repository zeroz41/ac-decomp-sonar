# Runs the seam audit over the translation units the build compiles (sonarlib docs/roadmap.md, M6 steps 6h, 10a to 10e
# and 9e3):
#   cmake -DSL_SEAM_AUDIT=<sl-seam-audit> -DPOLICY=<seam-policy.toml> -DCOMMANDS=<compile_commands.json> -DROOT=<root>
#         -P seam_audit_test.cmake
# It must exit 0, every file that uses GX listed, none of the types files calling and no entry stale, and print the
# line the port's notes give (NOTES.md, The seam audit), with no file transitional, as the seam is complete, so a file
# that starts using GX changes the test with the notes. No disc.
cmake_minimum_required(VERSION 3.25)

foreach(variable IN ITEMS SL_SEAM_AUDIT POLICY COMMANDS ROOT)
    if("${${variable}}" STREQUAL "")
        message(FATAL_ERROR "ac_seam_audit: ${variable} is not set")
    endif()
endforeach()

set(expected "seam GX transitional 0 calls 0 list 1 reference 4 types 11 unsupported 1 sdk 20 glue 4 unlisted 0 called 0")
string(APPEND expected " stale 0 skipped 238")
execute_process(COMMAND "${SL_SEAM_AUDIT}" --policy "${POLICY}" --commands "${COMMANDS}" --root "${ROOT}"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
message("exit ${result}:\n${output}${errors}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ac_seam_audit: the audit failed (${result})")
endif()
string(REGEX MATCH "seam GX [^\n]*" line "${output}")
if(NOT line STREQUAL expected)
    message(FATAL_ERROR "ac_seam_audit: the audit's line is not the notes':\n${line}\nexpected:\n${expected}")
endif()
