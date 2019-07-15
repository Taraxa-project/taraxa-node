include(cmake/scoped_include.cmake)
# ethash must be included before project() directive
scoped_include(cmake/submodules/ethash.cmake)

project(taraxa-node)
cmake_minimum_required(VERSION 3.13)

string(TOUPPER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_UPPER)
set(CMAKE_MACOSX_RPATH 1)

include(FindBoost)
include(FindProtobuf)
scoped_include(cmake/submodules/libff.cmake)
scoped_include(cmake/submodules/prometheus-cpp.cmake)
scoped_include(cmake/submodules/cryptopp.cmake)
scoped_include(cmake/submodules/secp256k1.cmake)

find_package(Boost)
find_package(Protobuf)

set(CMAKE_CXX_FLAGS_PREDEF ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE_UPPER}})
string(REPLACE " " ";" CMAKE_CXX_FLAGS_PREDEF "${CMAKE_CXX_FLAGS_PREDEF}")
list(REMOVE_DUPLICATES CMAKE_CXX_FLAGS_PREDEF)

set(CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE_UPPER} "") # disable auto predef flags
unset(CMAKE_CXX_STANDARD) # disable the problematic cmake std flag inference

set(CXX_STD_FLAG -std=c++17)
set(CXX_COMPILER_FLAGS ${CXX_STD_FLAG} ${CMAKE_CXX_FLAGS_PREDEF} -MMD -MF -MP)
string(JOIN " " CMAKE_CXX_FLAGS ${CXX_COMPILER_FLAGS})

# TODO: use protobuf_generate_cpp()
set(GRPC_OUT ${PROJECT_SOURCE_DIR}/grpc)
add_custom_target(protoc_taraxa_grpc
        COMMAND protoc -I. --grpc_out=${GRPC_OUT}
        --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin
        proto/taraxa_grpc.proto
        COMMAND protoc -I. --cpp_out=${GRPC_OUT}
        --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin
        proto/taraxa_grpc.proto
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Refresh protobuf...")

add_compile_definitions(BOOST_LOG_DYN_LINK ETH_FATDB)
include_directories(
        ${Boost_INCLUDE_DIR}
        ${Protobuf_INCLUDE_DIR}
        /usr/include/jsoncpp
        .
        submodules
        submodules/rapidjson/include
        submodules/libff
        submodules/libff/libff
        submodules/ethash/include
        concur_storage
        ${GRPC_OUT}
        submodules/prometheus-cpp/push/include
        submodules/prometheus-cpp/pull/include
        submodules/prometheus-cpp/core/include
        submodules/secp256k1/include)

add_link_options(${CXX_STD_FLAG} $<$<CONFIG:Debug>:-rdynamic>)
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
        z
        curl
        jsoncpp
        jsonrpccpp-common
        jsonrpccpp-server
        #       CMAKE library targets
        secp256k1
        cryptopp
        ethash
        ff
        prometheus-cpp::core
        prometheus-cpp::push
        prometheus-cpp::pull
)
if (APPLE)
    link_libraries(boost_log-mt)
endif (APPLE)

add_library(eth_core_objects OBJECT
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
        libdevcrypto/SecretStore.cpp)

add_library(p2p_objects OBJECT
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

add_library(web3jsonrpc_objects OBJECT
        libweb3jsonrpc/AdminNet.cpp
        libweb3jsonrpc/IpcServerBase.cpp
        libweb3jsonrpc/Net.cpp
        libweb3jsonrpc/SessionManager.cpp
        libweb3jsonrpc/UnixSocketServer.cpp
        libweb3jsonrpc/JsonHelper.cpp
        libweb3jsonrpc/Test.cpp
        libweb3jsonrpc/AdminTaraxa.cpp
        libweb3jsonrpc/Debug.cpp
        libweb3jsonrpc/Taraxa.cpp
        libweb3jsonrpc/Personal.cpp
        libweb3jsonrpc/RpcServer.cpp
        #	libweb3jsonrpc/JsonHelper.cpp
        #	libweb3jsonrpc/AccountHolder.pp
        )

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
        grpc_client.cpp
        grpc_server.cpp
        grpc_util.cpp
        transaction.cpp
        executor.cpp
        pbft_chain.cpp
        ${GRPC_OUT}/proto/taraxa_grpc.pb.cc
        ${GRPC_OUT}/proto/taraxa_grpc.grpc.pb.cc
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
        SimpleOverlayDBDelegate.cpp)

set(main_deps_objects
        $<TARGET_OBJECTS:eth_core_objects>
        $<TARGET_OBJECTS:p2p_objects>
        $<TARGET_OBJECTS:web3jsonrpc_objects>
        $<TARGET_OBJECTS:node_objects>)
add_library(main_objects OBJECT main.cpp)
add_executable(main ${main_deps_objects} $<TARGET_OBJECTS:main_objects>)

add_library(prometheus_demo_objects OBJECT prometheus_demo.cpp)
add_executable(prometheus_demo
        ${main_deps_objects}
        $<TARGET_OBJECTS:prometheus_demo_objects>)
add_custom_target(pdemo
        COMMAND prometheus_demo
        $(PUSHGATEWAY_IP) $(PUSHGATEWAY_PORT) $(PUSHGATEWAY_NAME)
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_library(keygen_objects OBJECT util_cmd/keygen.cpp)
add_executable(keygen
        $<TARGET_OBJECTS:eth_core_objects>
        $<TARGET_OBJECTS:keygen_objects>)

enable_testing()
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
set(ALL_TESTS
        ${CORE_TESTS}
        trie_test)

foreach (t ${CORE_TESTS})
    add_library(${t}_objects OBJECT core_tests/${t}.cpp)
endforeach ()
target_sources(concur_hash_test_objects PRIVATE
        concur_storage/concur_hash.cpp
        concur_storage/conflict_detector.cpp)

add_library(trie_test_objects OBJECT
        crypto_tests/MemTrie.cpp
        crypto_tests/trie_test.cpp)

foreach (t ${ALL_TESTS})
    add_executable(${t} ${main_deps_objects} $<TARGET_OBJECTS:${t}_objects>)
    add_dependencies(${t} main)
    add_test(NAME ${t} COMMAND ${t} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    list(APPEND ALL_TESTS_COMMANDS COMMAND ${t})
endforeach ()
add_custom_target(run_test
        ${ALL_TESTS_COMMANDS}
        DEPENDS ${ALL_TESTS}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})