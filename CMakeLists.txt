# Defines Taraxa library target.

project(taraxa-node)
cmake_minimum_required(VERSION 3.16)

set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)
# Set project properties like C++ standard, etc...
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 17)
#add_compile_options("-Wall" "-Werror") TODO: fix existing issues and uncomment this line

set(CMAKE_CXX_FLAGS_RELEASE "-O3")
# Optimization flag apparently needed to get rid of "File too big" assembler errors when compiling in Debug mode
# See: https://stackoverflow.com/questions/14125007/gcc-string-table-overflow-error-during-compilation/14601779#29479701
set(CMAKE_CXX_FLAGS_DEBUG "-g -O2") # TODO: originally -g -O0

# taraxad full static build option
option(TARAXA_STATIC_BUILD "Build taraxad as a static library (ON or OFF)" ON)
if (APPLE)
    message(STATUS "Disabling the static build because it's not supported on this OS")
    set(TARAXA_STATIC_BUILD OFF)
endif ()
message(STATUS "TARAXA_STATIC_BUILD: ${TARAXA_STATIC_BUILD}")

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# Use ccache to speed-up recompilation
find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
    message(STATUS "Building with ccache")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
else ()
    message(STATUS "Building without ccache")
endif ()

# Mac OS
if (DEFINED SYSTEM_HOME_OVERRIDE)
    list(PREPEND CMAKE_PREFIX_PATH ${SYSTEM_HOME_OVERRIDE})
endif ()

# Mac OS
set(CMAKE_OSX_SYSROOT /)
set(CMAKE_MACOSX_RPATH ON)

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
        GIT_HASH=\"${GIT_HASH}\"
        COMPILE_TIME=\"${COMPILE_TIME}\"
)

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
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Taraxa ${CPACK_PACKAGE_VERSION}")

    if (APPLE)
        set(CPACK_GENERATOR "DragNDrop")
    endif ()

    if (LINUX)
        # Linux gets a .tgz
        SET(CPACK_GENERATOR "TGZ")
        SET(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 1)
    endif (LINUX)

    include(CPack)
endif (ENABLE_PACKAGE_INSTALLER)

# Custom CMakeModules for finding libraries
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

# clang-tidy
include(CMakeModules/clang_tidy.cmake)

# clang-format
include(CMakeModules/clang_format.cmake)

# cppcheck
include(CMakeModules/cppcheck.cmake)

add_custom_target(check-static DEPENDS cpp-check clang-format-check)

# Add sub-directories cmakes
add_subdirectory(submodules)
add_subdirectory(src)
add_subdirectory(tests)

# An extension of this file that you can play with locally
include(local/CMakeLists_ext.cmake OPTIONAL)
