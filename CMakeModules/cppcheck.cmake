# additional target to perform cppcheck run, requires cppcheck
find_program(CPP_CHECK_EXE NAMES "cppcheck" DOC "Path to cppcheck executable")

if (NOT CPP_CHECK_EXE)
    message(STATUS "cppcheck not found.")
else ()
    message(STATUS "cppcheck found: ${CPP_CHECK_EXE}")

    add_custom_target(cpp-check ALL
            COMMAND ${CPP_CHECK_EXE}
            --error-exitcode=1
            --enable=all
            --suppress=missingInclude
            --suppress=useStlAlgorithm
            --suppress=noExplicitConstructor
            --suppress=unknownMacro
            --suppress=templateRecursion
            --suppress=uninitMemberVar src/libp2p/UPnP.cpp
            # This is only enabled because of test functions
            --suppress=unusedFunction
            # TODO remove this when we solve correct exit of programs
            --suppress=localMutex */main.cpp

            # Only show found errors
            "--quiet"

            # Optional: Specified C++ version
            "--std=c++20"

            # folder with source files to be checked
            ${PROJECT_SOURCE_DIR}/libraries
            ${PROJECT_SOURCE_DIR}/programs

            COMMENT "Analysing files in src/ by cppcheck"
            )
endif ()
