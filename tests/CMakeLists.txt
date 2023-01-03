add_subdirectory(test_util)

# Enable testing for current directory and below.
enable_testing(true)


add_executable(abi_test abi_test.cpp)
target_link_libraries(abi_test test_util)
add_test(abi_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/abi_test)

add_executable(cache_test cache_test.cpp)
target_link_libraries(cache_test test_util)
add_test(cache_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cache_test)

add_executable(crypto_test crypto_test.cpp)
target_link_libraries(crypto_test
    config
    vote
    ${MPFR_LIBRARIES}
    CONAN_PKG::gtest
)
add_test(crypto_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/crypto_test)

add_executable(dag_block_test dag_block_test.cpp)
target_link_libraries(dag_block_test test_util)
add_test(dag_block_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dag_block_test)

add_executable(dag_test dag_test.cpp)
target_link_libraries(dag_test test_util)
add_test(dag_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dag_test)

add_executable(final_chain_test final_chain_test.cpp)
target_link_libraries(final_chain_test test_util)
add_test(final_chain_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/final_chain_test)

add_executable(full_node_test full_node_test.cpp)
target_link_libraries(full_node_test test_util)
add_test(full_node_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/full_node_test)

# add_executable(hardfork_test hardfork_test.cpp)
# target_link_libraries(hardfork_test
#     core_libs
#     CONAN_PKG::gtest
# )
# add_test(hardfork_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hardfork_test)

add_executable(network_test network_test.cpp)
target_link_libraries(network_test test_util)
add_test(network_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/network_test)

add_executable(p2p_test p2p_test.cpp)
target_link_libraries(p2p_test test_util)
add_test(p2p_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/p2p_test)

add_executable(pbft_chain_test pbft_chain_test.cpp)
target_link_libraries(pbft_chain_test test_util)
add_test(pbft_chain_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pbft_chain_test)

add_executable(pbft_manager_test pbft_manager_test.cpp)
target_link_libraries(pbft_manager_test test_util)
add_test(pbft_manager_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pbft_manager_test)

add_executable(sortition_test sortition_test.cpp)
target_link_libraries(sortition_test test_util)
add_test(sortition_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sortition_test)

add_executable(state_api_test state_api_test.cpp)
target_link_libraries(state_api_test test_util)
add_test(state_api_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/state_api_test)

add_executable(transaction_test transaction_test.cpp)
target_link_libraries(transaction_test test_util)
add_test(transaction_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/transaction_test)

add_executable(vote_test vote_test.cpp)
target_link_libraries(vote_test test_util)
add_test(vote_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vote_test)


add_executable(tarcap_threadpool_test tarcap_threadpool_test.cpp)
target_link_libraries(tarcap_threadpool_test test_util)
add_test(tarcap_threadpool_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tarcap_threadpool_test)

add_executable(gas_pricer_test gas_pricer_test.cpp)
target_link_libraries(gas_pricer_test test_util)
add_test(gas_pricer_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/gas_pricer_test)

add_executable(rpc_test rpc_test.cpp)
target_link_libraries(rpc_test test_util)
add_test(rpc_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/gas_pricer_test)

add_custom_target(py_test)

add_custom_command(
    TARGET py_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/py
    COMMAND ./run.sh -s --tb=short --node_executable_path=${CMAKE_BINARY_DIR}/bin/taraxad
    COMMENT "Running Python tests")

add_test(py_test
  ${CMAKE_CURRENT_SOURCE_DIR}/py/run.sh
  -s
  --tb=short
  --node_executable_path=${CMAKE_BINARY_DIR}/bin/taraxad
)

set(GO_TEST_SCRIPT ${CMAKE_BINARY_DIR}/bin/go_test.sh)

# List serializes with ; delimiter by default. This is not working for bash
list(JOIN EVM_BUILD_ENV " " EVM_BUILD_ENV_STRING)

# Add ${GO_TEST_SCRIPT} to CMake dependencies to regenrate it if file was removed or changed for some reason
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${GO_TEST_SCRIPT})

# we need to write this command to file because add_test can't accept command
file(WRITE ${GO_TEST_SCRIPT}
"#!/bin/bash
cd ${CMAKE_SOURCE_DIR}/submodules/taraxa-evm/
export ${EVM_BUILD_ENV_STRING}
go test ./..."
)

# make script executable
execute_process(COMMAND chmod +x ${GO_TEST_SCRIPT})

add_test(go_test
  ${GO_TEST_SCRIPT}
)

add_custom_target(all_tests COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)