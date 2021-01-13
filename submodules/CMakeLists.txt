cmake_minimum_required(VERSION 3.16)

# TODO: get rid of submodules Makefile...
add_custom_target(
    submodules_build
    COMMAND $(MAKE)
    UPDATE_SUBMODULES=${UPDATE_SUBMODULES}
    DEPS_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
    SYSTEM_HOME_OVERRIDE=${SYSTEM_HOME_OVERRIDE}
    -f ${CMAKE_CURRENT_SOURCE_DIR}/Makefile_submodules.mk submodules
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

add_library(submodules INTERFACE)
add_dependencies(submodules submodules_build)

set(Submodules_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include" CACHE STRING "Submodules common include directory")
set(Submodules_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/lib" CACHE STRING "Submodules common binary directory")