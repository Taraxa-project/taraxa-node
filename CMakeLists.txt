cmake_minimum_required(VERSION 3.13)

project(taraxa-node)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 17)

set(IS_DEBUG 0)
if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(IS_DEBUG 1)
endif ()

macro(read_makefile_var var_name)
    execute_process(
        COMMAND
        make DEBUG=${IS_DEBUG} UPDATE_SUBMODULES=0 print_${var_name}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE ${var_name}
    )
    string(REPLACE " " ";" ${var_name} ${${var_name}})
    list(TRANSFORM ${var_name} STRIP)
endmacro()

read_makefile_var(COMPILE_DEFINITIONS)
add_compile_definitions(${COMPILE_DEFINITIONS})

read_makefile_var(INCLUDE_DIRS)
include_directories(${INCLUDE_DIRS})

read_makefile_var(LINK_FLAGS)
add_link_options(${LINK_FLAGS})

read_makefile_var(LIB_DIRS)
link_directories(${LIB_DIRS})

add_custom_target(
    submodules_build
    COMMAND
    $(MAKE) DEBUG=${IS_DEBUG} -f Makefile_submodules.mk submodules
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
add_library(submodules INTERFACE)
add_dependencies(submodules submodules_build)

read_makefile_var(LIBS)
read_makefile_var(OSX_FRAMEWORKS)
list(TRANSFORM OSX_FRAMEWORKS PREPEND "-framework ")
link_libraries(submodules ${LIBS} ${OSX_FRAMEWORKS})

read_makefile_var(NODE_SRCS)
add_library(node_objects OBJECT ${NODE_SRCS})
add_executable(main $<TARGET_OBJECTS:node_objects> src/main.cpp)

read_makefile_var(TEST_SRCS)
foreach (test_src ${TEST_SRCS})
    string(REPLACE "/" "_" __test_target__ ${test_src})
    get_filename_component(__test_target__ ${__test_target__} NAME_WE)
    add_executable(${__test_target__} $<TARGET_OBJECTS:node_objects> ${test_src})
    list(APPEND ALL_TESTS_TARGETS ${__test_target__})
    list(APPEND ALL_TESTS_EXE_PATHS $<TARGET_FILE:${__test_target__}>)
endforeach ()
add_custom_target(all_test DEPENDS ${ALL_TESTS_TARGETS})
add_custom_target(
    run_test
    COMMAND scripts/run_commands_long_circuit.sh ${ALL_TESTS_EXE_PATHS}
    DEPENDS all_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

add_custom_target(all_bin DEPENDS main all_test)

# An extension of this file that you can play with locally
include(local/CmakeLists_ext.cmake OPTIONAL)
