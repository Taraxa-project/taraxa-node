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
            #--suppress=unusedStructMember
            --suppress=stlIfFind
            #--suppress=identicalConditionAfterEarlyExit
            #--suppress=noCopyConstructor
            --suppress=noExplicitConstructor
            --suppress=unreadVariable
            --suppress=unknownMacro
            #--suppress=useInitializationList
            #--suppress=syntaxError
            --suppress=internalAstError # because of https://trac.cppcheck.net/ticket/9537 -> state_api.cpp, line 53, TODO: remove this

            # Only show found errors
            "--quiet"

            # Optional: Specified C++ version
            "--std=c++17"

            # folder with source files to be checked
            ${PROJECT_SOURCE_DIR}/src

            COMMENT "Analysing files in src/ by cppcheck"
            )
endif ()
