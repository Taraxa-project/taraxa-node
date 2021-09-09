# additional target to perform cppcheck run, requires cppcheck
find_program(CPP_CHECK_EXE NAMES "cppcheck" DOC "Path to cppcheck executable")

if (NOT CPP_CHECK_EXE)
    message(STATUS "cppcheck not found.")
else ()
    message(STATUS "cppcheck found: ${CPP_CHECK_EXE}")

    add_custom_target(cpp-check ALL
            COMMAND ${CPP_CHECK_EXE}
            --error-exitcode=1
            --enable=warning,style,performance,portability,information
            --suppress=missingInclude
            --suppress=useStlAlgorithm
            --suppress=shadowVariable
            --suppress=noExplicitConstructor
            --suppress=unreadVariable
            --suppress=unknownMacro
            --suppress=templateRecursion
            --suppress=uninitMemberVar src/libp2p/UPnP.cpp
            # Only show found errors
            "--quiet"

            # Optional: Specified C++ version
            "--std=c++20"

            # folder with source files to be checked
            ${PROJECT_SOURCE_DIR}/src

            COMMENT "Analysing files in src/ by cppcheck"
            )
endif ()
