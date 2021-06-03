# additional target to perform clang-format run, requires clang-format

if (APPLE)
    find_program(CLANG_FORMAT_EXE NAMES "clang-format" PATHS ${CMAKE_CURRENT_SOURCE_DIR}
            DOC "Path to clang-format executable"
            REQUIRED NO_DEFAULT_PATH
            )
else()
    find_program(CLANG_FORMAT_EXE NAMES "clang-format"
            DOC "Path to clang-format executable"
            REQUIRED
            )
endif()

if (NOT CLANG_FORMAT_EXE)
    message(STATUS "clang-format not found.")
else ()
    message(STATUS "clang-format found: ${CLANG_FORMAT_EXE}")

    # get all project files
    file(GLOB_RECURSE ALL_SOURCE_FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/*.[ch]pp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.[ch]pp)

    # TODO: print error/line
    add_custom_target(clang-format-check ALL
            COMMAND ! ${CLANG_FORMAT_EXE} -style=file -fallback-style=none
            --output-replacements-xml
            ${ALL_SOURCE_FILES}
            | grep -q "replacement offset"
            COMMENT "Checking source code formatting by clang-format"
            )

    add_custom_target(clang-format
            COMMAND ${CLANG_FORMAT_EXE} -style=file -i ${ALL_SOURCE_FILES}
            COMMENT "Formatting source code by clang-format"
            )

endif ()
