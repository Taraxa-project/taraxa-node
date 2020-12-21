cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 17)

# Enable testing for current directory and below.
enable_testing(true)

add_executable(pbft_manager_test pbft_manager_test.cpp)
target_link_libraries(pbft_manager_test app_base gtest)
add_test(pbft_manager_test pbft_manager_test)

add_executable(pbft_chain_test pbft_chain_test.cpp)
target_link_libraries(pbft_chain_test app_base gtest)
add_test(pbft_chain_test pbft_chain_test)

add_executable(network_test network_test.cpp)
target_link_libraries(network_test app_base gtest)
add_test(network_test network_test)

add_executable(transaction_test transaction_test.cpp)
target_link_libraries(transaction_test app_base gtest)
add_test(transaction_test transaction_test)

add_executable(dag_block_test dag_block_test.cpp)
target_link_libraries(dag_block_test app_base gtest)
add_test(dag_block_test dag_block_test)

add_executable(crypto_test crypto_test.cpp)
target_link_libraries(crypto_test app_base gtest)
add_test(crypto_test crypto_test)

add_executable(dag_test dag_test.cpp)
target_link_libraries(dag_test app_base gtest)
add_test(dag_test dag_test)

add_executable(p2p_test p2p_test.cpp)
target_link_libraries(p2p_test app_base gtest)
add_test(p2p_test p2p_test)

add_executable(pbft_rpc_test pbft_rpc_test.cpp)
target_link_libraries(pbft_rpc_test app_base gtest)
add_test(pbft_rpc_test pbft_rpc_test)

add_executable(full_node_test full_node_test.cpp)
target_link_libraries(full_node_test app_base taraxa_graphqlservice gtest gtest_main)
add_test(full_node_test full_node_test)


#todo: refactor
#add_subdirectory(../app_base ../tests/app_base)

#read_makefile_var(TEST_LIBS) -> gtest
#read_makefile_var(TEST_SRCS)

# TODO turn into object library once there are sources for it
# Can't do it now, because cmake doesn't allow libraries without sources
#read_makefile_var(TESTUTIL_SRCS)
#foreach (test_src ${TEST_SRCS})
#    string(REPLACE "${SRC_DIR}/" "" __test_target__ ${test_src})
#    string(REPLACE "/" "_" __test_target__ ${__test_target__})
#    get_filename_component(__test_target__ ${__test_target__} NAME_WE)
#    add_executable(${__test_target__}
#                   $<TARGET_OBJECTS:node_objects>
#                   ${TESTUTIL_SRCS}
#                   ${test_src})
#    target_link_libraries(${__test_target__} ${TEST_LIBS})
#    list(APPEND ALL_TESTS_TARGETS ${__test_target__})
#    list(APPEND ALL_TESTS_EXE_PATHS $<TARGET_FILE:${__test_target__}>)
#endforeach ()

#target_link_libraries(tests_full_node_test taraxa_graphqlservice)

#add_custom_target(all_test DEPENDS ${ALL_TESTS_TARGETS})
#add_custom_target(
#    run_test
#    COMMAND scripts/run_commands_long_circuit.sh ${ALL_TESTS_EXE_PATHS}
#    DEPENDS all_test
#    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
#)
#
#add_custom_target(all_bin DEPENDS main all_test)

# An extension of this file that you can play with locally
#include(local/CmakeLists_ext.cmake OPTIONAL)