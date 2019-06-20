set(CMAKE_MACOSX_RPATH 1)

include(cmake/scoped_include.cmake)
scoped_include(cmake/submodules/ethash.cmake) # ethash must be included before project() directive

project(taraxa-node)
cmake_minimum_required(VERSION 3.12) # actual minimal version might be lower, feel free to change

include(FindBoost)
include(FindProtobuf)
scoped_include(cmake/submodules/libff.cmake)
scoped_include(cmake/submodules/prometheus-cpp.cmake)
scoped_include(cmake/submodules/cryptopp.cmake)
scoped_include(cmake/submodules/secp256k1.cmake)
scoped_include(cmake/submodules/taraxa-evm.cmake)

find_package(Boost)
find_package(Protobuf)

set(CMAKE_CXX_STANDARD) # disable the problematic cmake std flag inference
set(CXX_STD_FLAG -std=c++17)
set(CXX_COMPILER_FLAGS ${CXX_STD_FLAG} -MMD -MP -MF)
string(JOIN " " CMAKE_CXX_FLAGS ${CXX_COMPILER_FLAGS} ${CMAKE_CXX_FLAGS})

add_compile_definitions(BOOST_LOG_DYN_LINK ETH_FATDB)
include_directories(
        ${Boost_INCLUDE_DIR}
        ${Protobuf_INCLUDE_DIR}
        .
        submodules
        submodules/rapidjson/include
        submodules/libff
        submodules/libff/libff
        submodules/ethash/include
        concur_storage
        grpc
        submodules/prometheus-cpp/push/include
        submodules/prometheus-cpp/pull/include
        submodules/prometheus-cpp/core/include
        submodules/secp256k1/include)

add_link_options(${CXX_STD_FLAG})
link_directories(${Boost_LIBRARY_DIRS})
link_libraries(
        #       Linker search path libraries
        boost_log
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
        prometheus-cpp::pull
)
if (APPLE)
    link_libraries(
            # TODO will be required for Go runtime
            #            "-framework CoreFoundation"
            #            "-framework Security"
            boost_log-mt)
endif (APPLE)

add_library(p2p_objects OBJECT
        libdevcore/Address.cpp
        libdevcore/Base64.cpp
        libdevcore/Common.cpp
        libdevcore/CommonData.cpp
        libdevcore/CommonIO.cpp
        libdevcore/CommonJS.cpp
        libdevcore/DBFactory.cpp
        libdevcore/FileSystem.cpp
        libdevcore/FixedHash.cpp
        libdevcore/Guards.cpp
        libdevcore/JsonUtils.cpp
        libdevcore/LevelDB.cpp
        libdevcore/Log.cpp
        libdevcore/LoggingProgramOptions.cpp
        libdevcore/MemoryDB.cpp
        libdevcore/OverlayDB.cpp
        libdevcore/RLP.cpp
        libdevcore/RocksDB.cpp
        libdevcore/SHA3.cpp
        libdevcore/StateCacheDB.cpp
        libdevcore/TransientDirectory.cpp
        libdevcore/TrieCommon.cpp
        libdevcore/TrieHash.cpp
        libdevcore/Worker.cpp
        libdevcrypto/AES.cpp
        libdevcrypto/Common.cpp
        libdevcrypto/CryptoPP.cpp
        libdevcrypto/Hash.cpp
        libdevcrypto/LibSnark.cpp
        libdevcrypto/SecretStore.cpp
        libp2p/CapabilityHost.cpp
        libp2p/Common.cpp
        libp2p/Host.cpp
        libp2p/Network.cpp
        libp2p/NodeTable.cpp
        libp2p/Peer.cpp
        libp2p/RLPXFrameCoder.cpp
        libp2p/RLPxHandshake.cpp
        libp2p/Session.cpp
        libp2p/UDP.cpp
        libp2p/UPnP.cpp)

add_library(node_objects OBJECT
        rocks_db.cpp
        dag_block.cpp
        util.cpp
        udp_buffer.cpp
        network.cpp
        full_node.cpp
        types.cpp
        visitor.cpp
        dag.cpp
        block_proposer.cpp
        rpc.cpp
        grpc_client.cpp
        grpc_server.cpp
        grpc_util.cpp
        transaction.cpp
        executor.cpp
        pbft_chain.cpp
        grpc/proto/taraxa_grpc.pb.cc
        grpc/proto/taraxa_grpc.grpc.pb.cc
        taraxa_capability.cpp
        libethereum/Account.cpp
        libethcore/LogEntry.cpp
        libethereum/TransactionReceipt.cpp
        libethereum/State.cpp
        sortition.cpp
        pbft_manager.cpp
        vote.cpp
        top.cpp
        config.cpp
        SimpleStateDBDelegate.cpp
        SimpleTaraxaRocksDBDelegate.cpp
        SimpleOverlayDBDelegate.cpp
        vm/TaraxaVM.cpp)

set(main_deps_objects $<TARGET_OBJECTS:node_objects> $<TARGET_OBJECTS:p2p_objects>)
add_library(main_objects OBJECT main.cpp)
add_executable(main ${main_deps_objects} $<TARGET_OBJECTS:main_objects>)

set(CORE_TESTS
        crypto_test
        pbft_rpc_test
        memorydb_test
        overlaydb_test
        statecachedb_test
        transaction_test
        dag_test
        concur_hash_test
        dag_block_test
        grpc_test
        full_node_test
        p2p_test
        network_test
        pbft_chain_test
        state_unit_tests
        pbft_manager_test)
foreach (t ${CORE_TESTS})
    add_library(${t}_objects OBJECT core_tests/${t}.cpp)
endforeach ()
target_sources(concur_hash_test_objects PRIVATE
        concur_storage/concur_hash.cpp
        concur_storage/conflict_detector.cpp)

add_library(trie_test_objects OBJECT
        crypto_tests/MemTrie.cpp
        crypto_tests/trie_test.cpp)

set(ALL_TESTS
        ${CORE_TESTS}
        trie_test)
foreach (t ${ALL_TESTS})
    add_executable(${t} ${main_deps_objects} $<TARGET_OBJECTS:${t}_objects>)
    add_test(NAME ${t} COMMAND ${t} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    list(APPEND ALL_TESTS_COMMANDS COMMAND ${t})
endforeach ()
add_custom_target(run_test
        ${ALL_TESTS_COMMANDS}
        DEPENDS main ${ALL_TESTS}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})