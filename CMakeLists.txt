add_subdirectory(submodules/ethash EXCLUDE_FROM_ALL)
option(WITH_PROCPS OFF)
add_subdirectory(submodules/libff EXCLUDE_FROM_ALL)
add_subdirectory(submodules/prometheus-cpp EXCLUDE_FROM_ALL)

project(taraxa-node)
cmake_minimum_required(VERSION 3.11)

include(FindBoost)
include(FindProtobuf)
include(cmake/submodules/cryptopp.cmake)
include(cmake/submodules/secp256k1.cmake)
include(cmake/submodules/taraxa_vm.cmake)

find_package(Boost)
find_package(Protobuf)

# TODO transfered from Makefile, but doesn't seem to be needed
#execute_process(
#        COMMAND
#        pkg-config --cflags protobuf grpc++ --libs protobuf grpc++
#        OUTPUT_VARIABLE GRPC_COMPILE_ARGS)
#
#add_link_options(-DBOOST_LOG_DYN_LINK -DETH_FATDB)
#add_compile_options(${GRPC_COMPILE_ARGS} -lgrpc++_reflection -ldl -I./grpc)

# Doesn't work for all compilers/platforms, sometimes need to use specific flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MACOSX_RPATH 1)
set(LOG_LIB boost_log-mt) # TODO do we really need this?
if (NOT APPLE)
    set(LOG_LIB boost_log)
    #    target_compile_definitions(main PUBLIC TODO transfered from Makefile, but doesn't seem to be needed
    #            CRYPTOPP_DISABLE_ASM)
endif ()

add_compile_options(-std=c++17 -c -O3 -MMD -MP -MF)
add_compile_definitions(BOOST_LOG_DYN_LINK ETH_FATDB)
add_link_options(-DBOOST_LOG_DYN_LINK)
execute_process(COMMAND echo ${Boost_INCLUDE_DIR})
execute_process(COMMAND echo ${Boost_LIBRARY_DIRS})
include_directories(
        ${Boost_INCLUDE_DIR}
        ${Protobuf_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/submodules
        ${PROJECT_SOURCE_DIR}/submodules/rapidjson/include
        ${PROJECT_SOURCE_DIR}/submodules/libff
        ${PROJECT_SOURCE_DIR}/submodules/libff/libff
        ${PROJECT_SOURCE_DIR}/submodules/ethash/include
        ${PROJECT_SOURCE_DIR}/concur_storage
        ${PROJECT_SOURCE_DIR}/grpc
        ${PROJECT_SOURCE_DIR}/submodules/prometheus-cpp/push/include
        ${PROJECT_SOURCE_DIR}/submodules/prometheus-cpp/pull/include
        ${PROJECT_SOURCE_DIR}/submodules/prometheus-cpp/core/include
        ${PROJECT_SOURCE_DIR}/submodules/secp256k1/include)

link_directories(${Boost_LIBRARY_DIRS})
link_libraries(
        #       Linker search path libraries
        ${LOG_LIB}
        dl
        pthread
        protobuf
        grpc++
        grpc++_reflection
        leveldb
        rocksdb
        gmp
        scrypt
        boost_program_options
        boost_filesystem
        boost_system
        boost_log_setup
        gtest
        boost_thread-mt
        rocksdb
        z
        curl
        #       CMAKE library targets
        taraxa_vm_cgo
        secp256k1
        cryptopp
        ethash
        ff
        prometheus-cpp::core
        prometheus-cpp::push
        prometheus-cpp::pull)
#if (APPLE) # TODO might be required for CGO in some circumstances (not sure which)
#    link_libraries("-framework CoreFoundation" "-framework Security")
#endif (APPLE)

set(p2p_SOURCES ${PROJECT_SOURCE_DIR}/libdevcore/Address.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/Base64.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/Common.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/CommonData.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/CommonIO.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/CommonJS.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/DBFactory.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/FileSystem.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/FixedHash.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/Guards.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/JsonUtils.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/LevelDB.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/Log.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/LoggingProgramOptions.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/MemoryDB.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/OverlayDB.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/RLP.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/RocksDB.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/SHA3.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/StateCacheDB.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/TransientDirectory.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/TrieCommon.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/TrieHash.cpp
        ${PROJECT_SOURCE_DIR}/libdevcore/Worker.cpp
        ${PROJECT_SOURCE_DIR}/libdevcrypto/AES.cpp
        ${PROJECT_SOURCE_DIR}/libdevcrypto/Common.cpp
        ${PROJECT_SOURCE_DIR}/libdevcrypto/CryptoPP.cpp
        ${PROJECT_SOURCE_DIR}/libdevcrypto/Hash.cpp
        ${PROJECT_SOURCE_DIR}/libdevcrypto/LibSnark.cpp
        ${PROJECT_SOURCE_DIR}/libdevcrypto/SecretStore.cpp
        ${PROJECT_SOURCE_DIR}/libp2p/CapabilityHost.cpp
        ${PROJECT_SOURCE_DIR}/libp2p/Common.cpp
        ${PROJECT_SOURCE_DIR}/libp2p/Host.cpp
        ${PROJECT_SOURCE_DIR}/libp2p/Network.cpp
        ${PROJECT_SOURCE_DIR}/libp2p/NodeTable.cpp
        ${PROJECT_SOURCE_DIR}/libp2p/Peer.cpp
        ${PROJECT_SOURCE_DIR}/libp2p/RLPXFrameCoder.cpp
        ${PROJECT_SOURCE_DIR}/libp2p/RLPxHandshake.cpp
        ${PROJECT_SOURCE_DIR}/libp2p/Session.cpp
        ${PROJECT_SOURCE_DIR}/libp2p/UDP.cpp
        ${PROJECT_SOURCE_DIR}/libp2p/UPnP.cpp)
add_library(node_obj OBJECT
        ${p2p_SOURCES}
        ${PROJECT_SOURCE_DIR}/rocks_db.cpp
        ${PROJECT_SOURCE_DIR}/dag_block.cpp
        ${PROJECT_SOURCE_DIR}/util.cpp
        ${PROJECT_SOURCE_DIR}/udp_buffer.cpp
        ${PROJECT_SOURCE_DIR}/network.cpp
        ${PROJECT_SOURCE_DIR}/full_node.cpp
        ${PROJECT_SOURCE_DIR}/types.cpp
        ${PROJECT_SOURCE_DIR}/visitor.cpp
        ${PROJECT_SOURCE_DIR}/dag.cpp
        ${PROJECT_SOURCE_DIR}/block_proposer.cpp
        ${PROJECT_SOURCE_DIR}/rpc.cpp
        ${PROJECT_SOURCE_DIR}/grpc_client.cpp
        ${PROJECT_SOURCE_DIR}/grpc_server.cpp
        ${PROJECT_SOURCE_DIR}/grpc_util.cpp
        ${PROJECT_SOURCE_DIR}/transaction.cpp
        ${PROJECT_SOURCE_DIR}/executor.cpp
        ${PROJECT_SOURCE_DIR}/pbft_chain.cpp
        ${PROJECT_SOURCE_DIR}/grpc/proto/taraxa_grpc.pb.cc
        ${PROJECT_SOURCE_DIR}/grpc/proto/taraxa_grpc.grpc.pb.cc
        ${PROJECT_SOURCE_DIR}/taraxa_capability.cpp
        ${PROJECT_SOURCE_DIR}/libethereum/Account.cpp
        ${PROJECT_SOURCE_DIR}/libethcore/LogEntry.cpp
        ${PROJECT_SOURCE_DIR}/libethereum/TransactionReceipt.cpp
        ${PROJECT_SOURCE_DIR}/libethereum/State.cpp
        ${PROJECT_SOURCE_DIR}/sortition.cpp
        ${PROJECT_SOURCE_DIR}/pbft_manager.cpp
        ${PROJECT_SOURCE_DIR}/vote.cpp
        ${PROJECT_SOURCE_DIR}/top.cpp
        ${PROJECT_SOURCE_DIR}/config.cpp
        ${PROJECT_SOURCE_DIR}/SimpleStateDBDelegate.cpp
        ${PROJECT_SOURCE_DIR}/SimpleTaraxaRocksDBDelegate.cpp
        ${PROJECT_SOURCE_DIR}/SimpleOverlayDBDelegate.cpp
        ${PROJECT_SOURCE_DIR}/vm/TaraxaVM.cpp)

add_executable(main ${PROJECT_SOURCE_DIR}/main.cpp)
target_link_libraries(main node_obj)

enable_testing()

set(CORE_TESTS
        concur_hash_test
        crypto_test
        dag_block_test
        dag_test
        full_node_test
        grpc_test
        long_network_test
        memorydb_test
        overlaydb_test
        p2p_test
        pbft_chain_test
        pbft_manager_test
        pbft_rpc_test
        state_unit_tests
        statecachedb_test
        transaction_test)
set(ALL_TESTS ${CORE_TESTS})
foreach (t ${CORE_TESTS})
    add_executable(${t} ${PROJECT_SOURCE_DIR}/core_tests/${t}.cpp)
    target_link_libraries(${t} node_obj)
    add_test(NAME ${t} COMMAND ${t} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endforeach ()

target_sources(concur_hash_test PUBLIC
        ${PROJECT_SOURCE_DIR}/concur_storage/concur_hash.cpp
        ${PROJECT_SOURCE_DIR}/concur_storage/conflict_detector.cpp)

add_custom_target(run_test
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target test
        DEPENDS ${ALL_TESTS})