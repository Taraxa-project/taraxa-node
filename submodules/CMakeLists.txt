cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 17)

# TODO: get rid of submodules Makefile...
add_custom_target(
        submodules_build
        COMMAND
        $(MAKE) DEBUG=${IS_DEBUG} DEPS_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR} -f ${CMAKE_CURRENT_SOURCE_DIR}/Makefile_submodules.mk submodules
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
add_library(submodules INTERFACE)
add_dependencies(submodules submodules_build)