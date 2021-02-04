# TODO: Break this cmake(app_base target) into smaller libraries with their own cmakes, e.g. network, chain, dag, etc... could be separate libraries

if (TARAXA_STATIC_BUILD)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a") # zlib does not support zlib_USE_STATIC_LIBS or similar approach
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")

    set(Boost_USE_STATIC_LIBS ON)
    set(jsoncpp_USE_STATIC_LIBS ON)
    set(jsonrpccpp_USE_STATIC_LIBS ON)
    set(crypto_USE_STATIC_LIBS ON)
    set(gmp_USE_STATIC_LIBS ON)
    set(mpfr_USE_STATIC_LIBS ON)
    set(scrypt_USE_STATIC_LIBS ON)
    set(rocksdb_USE_STATIC_LIBS ON)
    set(zstd_USE_STATIC_LIBS ON)
    set(bz2_USE_STATIC_LIBS ON)
    set(snappy_USE_STATIC_LIBS ON)
    set(lz4_USE_STATIC_LIBS ON)
endif (TARAXA_STATIC_BUILD)

# Find multithreaded boost components/libs
set(Boost_USE_MULTITHREADED ON)
set(BOOST_COMPONENTS)
list(APPEND BOOST_COMPONENTS program_options system filesystem thread log log_setup)

find_package(Boost 1.71 REQUIRED COMPONENTS ${BOOST_COMPONENTS})
if (Boost_FOUND)
    message(STATUS "Boost components found: ${BOOST_COMPONENTS}, Boost version: ${Boost_VERSION}")
else (Boost_FOUND)
    message(STATUS "Boost not found")
endif (Boost_FOUND)

# Find jsoncpp lib
find_package(jsoncpp REQUIRED)
find_package(jsonrpccpp REQUIRED)
find_package(crypto REQUIRED) # Find crypto lib # TODO: check if really required
find_package(gmp REQUIRED)
find_package(mpfr REQUIRED)
find_package(scrypt REQUIRED)
find_package(rocksdb REQUIRED)
find_package(zstd REQUIRED)
find_package(bz2 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(snappy REQUIRED)
find_package(lz4 REQUIRED)

set(SOURCES
        # ---- public headers -----
        util/util.hpp
        node/full_node.hpp
        transaction_manager/transaction_manager.hpp
        storage/db_storage.hpp
        dag/dag.hpp
        consensus/pbft_chain.hpp
        aleth/node_api.hpp
        aleth/filter_api.hpp
        aleth/database.hpp
        aleth/pending_block.hpp
        aleth/state_api.hpp
        util/util_json.hpp
        consensus/block_proposer.hpp
        transaction_manager/transaction_status.hpp
        chain/chain_config.hpp
        consensus/vrf_wrapper.hpp
        node/executor.hpp
        network/network.hpp
        common/static_init.hpp
        dag/vdf_sortition.hpp
        chain/final_chain.hpp
        consensus/pbft_config.hpp
        network/taraxa_capability.hpp
        network/http_server.hpp
        util/exit_stack.hpp
        util/simple_event.hpp
        util/encoding_rlp.hpp
        util/range_view.hpp
        util/lazy.hpp
        config/config.hpp
        node/replay_protection_service.hpp
        common/types.hpp
        dag/dag_block.hpp
        network/rpc/Taraxa.h
        network/rpc/Test.h
        network/rpc/TaraxaClient.h
        network/rpc/NetClient.h
        network/rpc/Net.h
        network/rpc/NetFace.h
        network/rpc/TaraxaFace.h
        network/rpc/TestFace.h
        network/rpc/jsonrpc_server.hpp
        network/rpc/WSServer.h
        network/rpc/TestClient.h
        transaction_manager/transaction_order_manager.hpp
        consensus/vote.hpp
        transaction_manager/transaction.hpp
        transaction_manager/transaction_queue.hpp
        logger/logger_config.hpp
        logger/log.hpp
        chain/state_api.hpp
        dag/dag_block_manager.hpp
        consensus/pbft_manager.hpp

        # ---- private sources -----
        transaction_manager/transaction.cpp
        consensus/pbft_manager.cpp
        storage/db_storage.cpp
        consensus/vrf_wrapper.cpp
        aleth/node_api.cpp
        aleth/pending_block.cpp
        aleth/filter_api.cpp
        aleth/database.cpp
        aleth/state_api.cpp
        node/executor.cpp
        network/taraxa_capability.cpp
        network/http_server.cpp
        transaction_manager/transaction_manager.cpp
        dag/vdf_sortition.cpp
        chain/chain_config.cpp
        chain/final_chain.cpp
        consensus/pbft_config.cpp
        consensus/vote.cpp
        network/network.cpp
        util/util.cpp
        consensus/pbft_chain.cpp
        node/replay_protection_service.cpp
        config/config.cpp
        dag/dag_block.cpp
        node/full_node.cpp
        consensus/block_proposer.cpp
        network/rpc/Taraxa.cpp
        network/rpc/Test.cpp
        network/rpc/WSServer.cpp
        network/rpc/Net.cpp
        network/rpc/jsonrpc_server.cpp
        transaction_manager/transaction_order_manager.cpp
        chain/state_api.cpp
        transaction_manager/transaction_queue.cpp
        dag/dag.cpp
        logger/logger_config.cpp
        logger/log.cpp
        common/types.cpp
        dag/dag_block_manager.cpp
        )

add_library(app_base STATIC ${SOURCES})
target_include_directories(app_base PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${Boost_INCLUDE_DIR}
        ${jsoncpp_INCLUDE_DIRS}
        )

target_link_libraries(app_base
        taraxa_graphqlservice
        submodules
        pthread
        #ssl
        ${crypto_LIBRARIES} # TODO: is this lib even required ?
        ${CMAKE_DL_LIBS}    # because of libcrypto
        ${mpfr_LIBRARIES}
        ${gmp_LIBRARIES}
        ${Boost_LIBRARIES}
        ${rocksdb_LIBRARIES}
        ${snappy_LIBRARIES} # rocksdb dep
        ${zstd_LIBRARIES}   # rocksdb dep
        ${bz2_LIBRARIES}    # rocksdb dep
        ${ZLIB_LIBRARIES}   # rocksdb dep
        ${lz4_LIBRARIES}    # rocksdb dep
        ${scrypt_LIBRARIES}
        ${jsoncpp_LIBRARIES}
        ${jsonrpccpp_SERVER_LIBRARIES}
        )

add_subdirectory(network/graphql)
add_subdirectory(network/rpc)

# needed for golang runtime that comes together with taraxa-evm
if (APPLE)  # if "APPLE" not sufficient, use if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(app_base "-framework CoreFoundation")
    target_link_libraries(app_base "-framework Security")
else ()
    target_link_libraries(app_base stdc++fs)
endif ()

# Main taraxad binary
add_subdirectory(taraxad)