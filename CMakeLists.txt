# Defines Taraxa library target.

# Use ccache to speed-up recompilation
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Building with ccache")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
else()
    message(STATUS "Building without ccache")
endif()

project(taraxa-node)
cmake_minimum_required(VERSION 3.16)

# Set project properties like C++ standard, etc...
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 17)
#add_compile_options("-Wall" "-Werror") TODO: fix existing issues and uncomment this line

set(CMAKE_CXX_FLAGS_RELEASE "-O3")
# Optimization flag apparently needed to get rid of "File too big" assembler errors when compiling in Debug mode
# See: https://stackoverflow.com/questions/14125007/gcc-string-table-overflow-error-during-compilation/14601779#29479701
set(CMAKE_CXX_FLAGS_DEBUG "-g -O2") # TODO: originally -g -O0

# Get the latest abbreviated commit hash of the working branch
execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get compile time
execute_process(
    COMMAND date -u +%Y-%m-%d-%H:%M:%S
    OUTPUT_VARIABLE COMPILE_TIME
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_compile_definitions(
    BOOST_ALL_DYN_LINK
    BOOST_SPIRIT_THREADSAFE
    GIT_HASH=\"${GIT_HASH}\"
    COMPILE_TIME=\"${COMPILE_TIME}\"
)

# Find clang-tidy for code static analysis
# TODO: clang-tidy lint level set to LOW or more.
set(taraxa_LINT_LEVEL "OFF" CACHE STRING "Lint level during taraxa build (FULL, HIGH, LOW, OFF)")
find_program(
    CLANG_TIDY_EXE
    NAMES "clang-tidy"
    DOC "Path to clang-tidy executable"
)

if(NOT CLANG_TIDY_EXE)
    message(STATUS "clang-tidy not found")
elseif(VERSION LESS 3.6)
    messgae(STATUS "clang-tidy found but only supported with CMake version >= 3.6")
else()
    message(STATUS "clany-tidy found: ${CLANG_TIDY_EXE}")
    if("${taraxa_LINT_LEVEL}" STREQUAL "FULL")
        message(STATUS "Linting level set to: FULL")
        set(DO_CLANG_TIDY "${CLANG_TIDY_EXE}" "-checks='*'")
    elseif("${taraxa_LINT_LEVEL}" STREQUAL "HIGH")
        message(STATUS "Linting level set to: HIGH")
        set(DO_CLANG_TIDY "${CLANG_TIDY_EXE}" "-checks='boost-use-to-string,clang-analyzer-*,cppcoreguidelines-*,llvm-*,misc-*,performance-*,readability-*'")
    elseif("${taraxa_LINT_LEVEL}" STREQUAL "LOW")
        message(STATUS "Linting level set to: LOW")
        set(DO_CLANG_TIDY "${CLANG_TIDY_EXE}" "-checks='clang-analyzer-*'")
    else()
        unset(CLANG_TIDY_EXE)
        message(STATUS "Linting level set to: OFF")
    endif()
endif(NOT CLANG_TIDY_EXE)

# Enable creation of taraxa package containing taraxad binary
set(ENABLE_PACKAGE_INSTALLER TRUE CACHE BOOL "Build Taraxa package")

if (ENABLE_PACKAGE_INSTALLER)
    set(VERSION_MAJOR 1)
    set(VERSION_MINOR 0)
    set(VERSION_PATCH 0)

    include(InstallRequiredSystemLibraries)

    set(CPACK_OUTPUT_FILE_PREFIX ${CMAKE_BINARY_DIR}/packages)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)

    SET(CPACK_PACKAGE_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
    set(CPACK_PACKAGE_NAME "Taraxa")
    set(CPACK_PACKAGE_VENDOR "Phragmites Inc.")
    set(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
    set(CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
    set(CPACK_PACKAGE_VERSION_PATCH "${VERSION_PATCH}")
    set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
    set(CPACK_PACKAGE_DESCRIPTION "A client for the Taraxa network")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A client for the Taraxa network")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Taraxa ${CPACK_PACKAGE_VERSION}")

    if(APPLE)
        set(CPACK_GENERATOR "DragNDrop")
    endif()

    if(LINUX)
        # Linux gets a .tgz
        SET(CPACK_GENERATOR "TGZ")
        SET(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 1)
    endif(LINUX)

    include(CPack)
endif(ENABLE_PACKAGE_INSTALLER)

# clang-format
include(clang_format.cmake)

# Add sub-directories cmakes
add_subdirectory(submodules)
add_subdirectory(src)
add_subdirectory(tests)
