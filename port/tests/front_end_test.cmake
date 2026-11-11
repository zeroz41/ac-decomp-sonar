# Checks that ac_port links no GX front end (sonarlib docs/steps/m6-10e.md): the frame's seams record on sonarlib's
# kits, and only ac_port_reference translates GX.
#   cmake -DNM=<nm> -DAC_PORT=<ac_port> -P front_end_test.cmake
# The symbol listing must hold the jsys kit's clear, so an empty or stripped listing fails, and no symbol may start
# with sl_gxf_. No disc.
cmake_minimum_required(VERSION 3.25)

foreach(variable IN ITEMS NM AC_PORT)
    if("${${variable}}" STREQUAL "")
        message(FATAL_ERROR "ac_port_front_end: ${variable} is not set")
    endif()
endforeach()

execute_process(COMMAND "${NM}" "${AC_PORT}" RESULT_VARIABLE result OUTPUT_VARIABLE symbols ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ac_port_front_end: ${NM} failed (${result}):\n${errors}")
endif()
if(NOT symbols MATCHES "[ \t]sl_jsys_display_clear\n")
    message(FATAL_ERROR "ac_port_front_end: ac_port's symbols do not hold sl_jsys_display_clear")
endif()
string(REGEX MATCHALL "[ \t]sl_gxf_[A-Za-z0-9_]*" front_end "${symbols}")
if(front_end)
    list(REMOVE_DUPLICATES front_end)
    string(REPLACE ";" "\n" front_end "${front_end}")
    message(FATAL_ERROR "ac_port_front_end: ac_port links the GX front end:\n${front_end}")
endif()
message("ac_port_front_end: no sl_gxf_ symbol")
