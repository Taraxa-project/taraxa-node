# A perfectly functioning cmake build based on the make build.
# can be used to build the project and for IDE integration.
# Tested on ubuntu 20.04, but should be fine on Mac too.

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

read_makefile_var(SRC_DIR)
read_makefile_var(INCLUDE_DIRS)
include_directories(${SRC_DIR} ${INCLUDE_DIRS})

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

add_subdirectory(src/net/graphql)

read_makefile_var(NODE_SRCS)
add_library(node_objects OBJECT ${NODE_SRCS})
add_executable(main $<TARGET_OBJECTS:node_objects> src/main.cpp)

read_makefile_var(TEST_LIBS)
read_makefile_var(TEST_SRCS)
# TODO turn into object library once there are sources for it
# Can't do it now, because cmake doesn't allow libraries without sources
read_makefile_var(TESTUTIL_SRCS)
foreach (test_src ${TEST_SRCS})
    string(REPLACE "${SRC_DIR}/" "" __test_target__ ${test_src})
    string(REPLACE "/" "_" __test_target__ ${__test_target__})
    get_filename_component(__test_target__ ${__test_target__} NAME_WE)
    add_executable(${__test_target__}
                   $<TARGET_OBJECTS:node_objects>
                   ${TESTUTIL_SRCS}
                   ${test_src})
    target_link_libraries(${__test_target__} ${TEST_LIBS})
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


#set(SOURCES
#        # ---- public headers -----
#        src/util.hpp
#        src/util_test/gtest.hpp
#        src/util_test/util.hpp
#        src/util_test/samples.hpp
#        src/full_node.hpp
#        src/transaction_manager.hpp
#        src/db_storage.hpp
#        src/dag.hpp
#        src/pbft_chain.hpp
#        src/aleth/node_api.hpp
#        src/aleth/filter_api.hpp
#        src/aleth/database.hpp
#        src/aleth/pending_block.hpp
#        src/aleth/state_api.hpp
#        src/util_json.hpp
#        src/block_proposer.hpp
#        src/transaction_status.hpp
#        src/chain_config.hpp
#        src/vrf_wrapper.hpp
#        src/executor.hpp
#        src/network.hpp
#        src/static_init.hpp
#        src/vdf_sortition.hpp
#        src/final_chain.hpp
#        src/pbft_config.hpp
#        src/sortition.hpp
#        src/taraxa_capability.hpp
#        src/util/exit_stack.hpp
#        src/util/simple_event.hpp
#        src/util/encoding_rlp.hpp
#        src/util/range_view.hpp
#        src/util/lazy.hpp
#        src/config.hpp
#        src/replay_protection_service.hpp
#        src/types.hpp
#        src/dag_block.hpp
#        src/net/Taraxa.h
#        src/net/Test.h
#        src/net/TaraxaClient.h
#        src/net/NetClient.h
#        src/net/Net.h
#        src/net/NetFace.h
#        src/net/TaraxaFace.h
#        src/net/TestFace.h
#        src/net/graphql/gen/TodaySchema.h
#        src/net/graphql/TodayMock.h
#        src/net/RpcServer.h
#        src/net/WSServer.h
#        src/net/TestClient.h
#        src/transaction_order_manager.hpp
#        src/vote.hpp
#        src/transaction.hpp
#        src/transaction_queue.hpp
#        src/logger/Config.hpp
#        src/logger/log.hpp
#        src/state_api.hpp
#        src/pbft_manager.hpp
#
#
#        # ---- private sources -----
#        src/sortition.cpp
#        src/transaction.cpp
#        src/tests/pbft_manager_test.cpp
#        src/tests/pbft_chain_test.cpp
#        src/tests/network_test.cpp
#        src/tests/transaction_test.cpp
#        src/tests/dag_block_test.cpp
#        src/tests/full_node_test.cpp
#        src/tests/crypto_test.cpp
#        src/tests/dag_test.cpp
#        src/tests/p2p_test.cpp
#        src/tests/pbft_rpc_test.cpp
#        src/pbft_manager.cpp
#        src/db_storage.cpp
#        src/vrf_wrapper.cpp
#        src/aleth/node_api.cpp
#        src/aleth/pending_block.cpp
#        src/aleth/filter_api.cpp
#        src/aleth/database.cpp
#        src/aleth/state_api.cpp
#        src/executor.cpp
#        src/taraxa_capability.cpp
#        src/main.cpp
#        src/transaction_manager.cpp
#        src/vdf_sortition.cpp
#        src/chain_config.cpp
#        src/final_chain.cpp
#        src/pbft_config.cpp
#        src/final_chain_test.cpp
#        src/replay_protection_service_test.cpp
#        src/vote.cpp
#        src/state_api_test.cpp
#        src/network.cpp
#        src/util.cpp
#        src/pbft_chain.cpp
#        src/replay_protection_service.cpp
#        src/config.cpp
#        src/dag_block.cpp
#        src/full_node.cpp
#        src/block_proposer.cpp
#        src/net/Taraxa.cpp
#        src/net/Test.cpp
#        src/net/WSServer.cpp
#        src/net/Net.cpp
#        src/net/graphql/TodayMock.cpp
#        src/net/graphql/benchmark.cpp
#        src/net/graphql/gen/TodaySchema.cpp
#        src/net/RpcServer.cpp
#        src/transaction_order_manager.cpp
#        src/state_api.cpp
#        src/transaction_queue.cpp
#        src/dag.cpp
#        src/logger/Config.cpp
#        src/logger/log.cpp
#        src/types.cpp
#        )