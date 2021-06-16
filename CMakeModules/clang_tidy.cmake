# additional target to perform clang-tidy run, requires clang-tidy

message(STATUS "clang-tidy LLVM_VERSION: ${LLVM_VERSION}")

find_program(CLANG_TIDY_EXE
        NAMES clang-tidy-${LLVM_VERSION} clang-tidy
        DOC "Path to clang-tidy executable"
        REQUIRED
        )

set(taraxa_LINT_LEVEL "OFF" CACHE STRING "Lint level during taraxa build (FULL, HIGH, LOW, OFF)")

if (NOT CLANG_TIDY_EXE)
    message(STATUS "clang-tidy not found.")
elseif (VERSION LESS 3.6)
    message(STATUS "clang-tidy found but only supported with CMake version >= 3.6")
else ()
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")

    # TODO: configure properly clang-tidy
    if ("${taraxa_LINT_LEVEL}" STREQUAL "FULL")
        message(STATUS "Linting level set to: FULL")
        set(CLANG_TIDY_CHECKS "-checks='*'")
    elseif ("${taraxa_LINT_LEVEL}" STREQUAL "HIGH")
        message(STATUS "Linting level set to: HIGH")
        set(CLANG_TIDY_CHECKS "-checks='boost-use-to-string,clang-analyzer-*,cppcoreguidelines-*,llvm-*,misc-*,performance-*,readability-*'")
    elseif ("${taraxa_LINT_LEVEL}" STREQUAL "LOW")
        message(STATUS "Linting level set to: LOW")
        set(CLANG_TIDY_CHECKS "-checks='clang-analyzer-*'")
    else ()
        unset(CLANG_TIDY_EXE)
        message(STATUS "Linting level set to: OFF")
    endif ()

    add_custom_target(clang-tidy
            COMMAND ${CLANG_TIDY_EXE} ${CLANG_TIDY_CHECKS}
            COMMENT "Checking source code by clang-tidy"
            )

endif ()
