include_guard()

set(PCH_PROGRAM ${CMAKE_CURRENT_LIST_DIR}/pch.py)
option(WITH_PCH "Use precompiled headers technique" OFF)
message(STATUS "WITH_PCH: ${WITH_PCH}")

macro(pch_gen basedir_ dest_file_ exceptions_)
    if (WITH_PCH)
        execute_process(COMMAND ${PCH_PROGRAM} ${dest_file_} ${exceptions_} WORKING_DIRECTORY ${basedir_})
    endif ()
endmacro()

macro(pch_link target_ visibility_ dest_file_)
    if (WITH_PCH)
        target_precompile_headers(${target_} ${visibility_} ${dest_file_})
    endif ()
endmacro()