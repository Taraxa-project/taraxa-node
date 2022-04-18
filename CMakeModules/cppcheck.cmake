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
            --suppress=unsignedPositive
            # false positive
            --suppress=uninitMemberVar:${PROJECT_SOURCE_DIR}/*/UPnP.cpp
            # This is only enabled because of test functions and false positives
            --suppress=unusedFunction:${PROJECT_SOURCE_DIR}/*/Eth.cpp
            --suppress=unusedFunction:${PROJECT_SOURCE_DIR}/*/Common.cpp
            --suppress=unusedFunction:${PROJECT_SOURCE_DIR}/*/Base64.cpp
            --suppress=unusedFunction:${PROJECT_SOURCE_DIR}/*/util.cpp
            --suppress=unusedFunction:${PROJECT_SOURCE_DIR}/*/vrf_wrapper.cpp
            --suppress=unusedFunction:${PROJECT_SOURCE_DIR}/*/UPnP.cpp
            --suppress=unusedFunction:${PROJECT_SOURCE_DIR}/*/logger.cpp
            --suppress=unusedFunction:${PROJECT_SOURCE_DIR}/*/replay_protection_service.cpp
            # TODO remove this when we solve correct exit of programs
            --suppress=localMutex:${PROJECT_SOURCE_DIR}/*/main.cpp
            # error: Rethrowing current exception with 'throw;', it seems there is no current exception to rethrow.
            # false positive
            --suppress=rethrowNoCurrentException:${PROJECT_SOURCE_DIR}/*/rpc_error_handler.cpp
            # Just style warning
            --suppress=virtualCallInConstructor:${PROJECT_SOURCE_DIR}/*/final_chain.cpp

            # Only show found errors
            "--quiet"

            # Optional: Specified C++ version
            "--std=c++20"

            # folder with source files to be checked
            ${PROJECT_SOURCE_DIR}/libraries
            ${PROJECT_SOURCE_DIR}/programs

            COMMENT "Analysing files by cppcheck .."
            )
endif ()
