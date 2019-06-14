set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MACOSX_RPATH 1)

add_subdirectory(submodules/ethash EXCLUDE_FROM_ALL)
option(WITH_PROCPS OFF)
add_subdirectory(submodules/libff EXCLUDE_FROM_ALL)
add_subdirectory(submodules/prometheus-cpp EXCLUDE_FROM_ALL)

project(taraxa-node)
cmake_minimum_required(VERSION 3.11)

include(FindBoost)
include(FindProtobuf)
include(cmake/cryptopp.cmake)
include(cmake/secp256k1.cmake)

find_package(Boost)
find_package(Protobuf)

#execute_process(
#        COMMAND
#        pkg-config --cflags protobuf grpc++ --libs protobuf grpc++
#        OUTPUT_VARIABLE GRPC_COMPILE_ARGS)
#
#add_link_options(-DBOOST_LOG_DYN_LINK -DETH_FATDB)

#set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} ${GRPC_COMPILE_ARGS})
#add_compile_options(${GRPC_COMPILE_ARGS} -lgrpc++_reflection -ldl -I./grpc)

set(LOG_LIB boost_log-mt)
if (NOT APPLE)
    set(LOG_LIB boost_log)
endif ()
set(SOURCES_p2p
        ${PROJECT_SOURCE_DIR}/libdevcore/Address.cpp
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
set(SOURCES_taraxa
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
        ${PROJECT_SOURCE_DIR}/SimpleOverlayDBDelegate.cpp)
add_executable(main ${SOURCES_p2p} ${SOURCES_taraxa} ${PROJECT_SOURCE_DIR}/main.cpp)
target_compile_definitions(main PUBLIC
        BOOST_LOG_DYN_LINK
        ETH_FATDB)
target_compile_options(main PUBLIC
        -std=c++17 -c -O3 -MMD -MP -MF)
target_include_directories(main PUBLIC
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
target_link_directories(main PUBLIC
        ${Boost_LIBRARY_DIRS})
target_link_libraries(main
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
        secp256k1
        cryptopp
        ethash
        ff
        prometheus-cpp::core
        prometheus-cpp::push
        prometheus-cpp::pull)