# Enable testing for current directory and below.
enable_testing(true)

add_executable(pbft_manager_test pbft_manager_test.cpp)
target_link_libraries(pbft_manager_test app_base CONAN_PKG::gtest)
add_test(pbft_manager_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pbft_manager_test)

add_executable(pbft_chain_test pbft_chain_test.cpp)
target_link_libraries(pbft_chain_test app_base CONAN_PKG::gtest)
add_test(pbft_chain_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pbft_chain_test)

add_executable(network_test network_test.cpp)
target_link_libraries(network_test app_base CONAN_PKG::gtest)
add_test(network_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/network_test)

add_executable(transaction_test transaction_test.cpp)
target_link_libraries(transaction_test app_base CONAN_PKG::gtest)
add_test(transaction_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/transaction_test)

add_executable(dag_block_test dag_block_test.cpp)
target_link_libraries(dag_block_test app_base CONAN_PKG::gtest)
add_test(dag_block_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dag_block_test)

add_executable(crypto_test crypto_test.cpp)
target_link_libraries(crypto_test app_base CONAN_PKG::gtest)
add_test(crypto_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/crypto_test)

add_executable(dag_test dag_test.cpp)
target_link_libraries(dag_test app_base CONAN_PKG::gtest)
add_test(dag_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dag_test)

add_executable(p2p_test p2p_test.cpp)
target_link_libraries(p2p_test app_base CONAN_PKG::gtest)
add_test(p2p_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/p2p_test)

add_executable(vote_test vote_test.cpp)
target_link_libraries(vote_test app_base CONAN_PKG::gtest)
add_test(vote_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vote_test)

add_executable(full_node_test full_node_test.cpp)
target_link_libraries(full_node_test app_base CONAN_PKG::gtest)
add_test(full_node_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/full_node_test)

add_executable(replay_protection_service_test replay_protection_service_test.cpp)
target_link_libraries(replay_protection_service_test app_base CONAN_PKG::gtest)
add_test(replay_protection_service_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/replay_protection_service_test)

add_executable(final_chain_test final_chain_test.cpp)
target_link_libraries(final_chain_test app_base CONAN_PKG::gtest)
add_test(final_chain_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/final_chain_test)

add_executable(state_api_test state_api_test.cpp)
target_link_libraries(state_api_test app_base CONAN_PKG::gtest)
add_test(state_api_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/state_api_test)

add_executable(rpc_taraxa_test rpc_taraxa_test.cpp)
target_link_libraries(rpc_taraxa_test app_base CONAN_PKG::gtest)
add_test(rpc_taraxa_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rpc_taraxa_test)

add_custom_target(all_tests COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)