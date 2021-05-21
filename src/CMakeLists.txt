# TODO: Break this cmake(app_base target) into smaller libraries with their own cmakes, e.g. network, chain, dag, etc... could be separate libraries

set(SOURCES
        # ---- public headers -----
        util/util.hpp
        node/full_node.hpp
        transaction_manager/transaction_manager.hpp
        storage/db_storage.hpp
        dag/dag.hpp
        consensus/pbft_chain.hpp
        aleth/node_api.hpp
        aleth/database.hpp
        aleth/state_api.hpp
        aleth/dummy_eth_apis.hpp
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
        util/exit_stack.hpp
        util/encoding_rlp.hpp
        util/range_view.hpp
        util/lazy.hpp
        config/config.hpp
        node/replay_protection_service.hpp
        common/types.hpp
        dag/dag_block.hpp
        network/packets_stats.hpp
        network/rpc/Taraxa.h
        network/rpc/Test.h
        network/rpc/TaraxaClient.h
        network/rpc/NetClient.h
        network/rpc/Net.h
        network/rpc/NetFace.h
        network/rpc/TaraxaFace.h
        network/rpc/TestFace.h
        network/rpc/RpcServer.h
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
        dag/proposal_period_levels_map.hpp
        consensus/pbft_manager.hpp

        # ---- private sources -----
        util/thread_pool.cpp
        transaction_manager/transaction.cpp
        consensus/pbft_manager.cpp
        storage/db_storage.cpp
        consensus/vrf_wrapper.cpp
        aleth/node_api.cpp
        aleth/database.cpp
        aleth/state_api.cpp
        node/executor.cpp
        network/taraxa_capability.cpp
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
        network/packets_stats.cpp
        network/rpc/Taraxa.cpp
        network/rpc/Test.cpp
        network/rpc/WSServer.cpp
        network/rpc/Net.cpp
        network/rpc/RpcServer.cpp
        transaction_manager/transaction_order_manager.cpp
        chain/state_api.cpp
        transaction_manager/transaction_queue.cpp
        dag/dag.cpp
        logger/logger_config.cpp
        logger/log.cpp
        common/types.cpp
        dag/dag_block_manager.cpp
        dag/proposal_period_levels_map.cpp
        util/jsoncpp.cpp
        network/rpc/rpc_error_handler.cpp
        )

add_subdirectory(network/rpc)

add_library(app_base STATIC ${SOURCES})

# TODO: have some problems with include dirs for imported targets
get_target_property(TARAXA_VDF_INCUDE taraxa-vdf INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(TARAXA_VRF_INCUDE taraxa-vrf INTERFACE_INCLUDE_DIRECTORIES)

target_include_directories(app_base PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${TARAXA_VDF_INCUDE}
        ${TARAXA_VRF_INCUDE}
        )

# system libs should be included via -isystem so compiler knows how to
# treat them. e.g. do not check for warnings
target_include_directories(app_base SYSTEM PUBLIC
        ${CONAN_INCLUDE_DIRS}
        ${jsoncpp_INCLUDE_DIRS}
        )

target_link_libraries(app_base
        taraxa-vrf
        taraxa-vdf
        taraxa-evm
        pthread
        ${scrypt_LIBRARIES}
        ${jsoncpp_LIBRARIES}
        ${jsonrpccpp_SERVER_LIBRARIES}
        CONAN_PKG::boost
        CONAN_PKG::openssl
        CONAN_PKG::gmp
        CONAN_PKG::snappy
        CONAN_PKG::zstd
        CONAN_PKG::lz4
        CONAN_PKG::rocksdb
        CONAN_PKG::bzip2
        CONAN_PKG::zlib
        CONAN_PKG::mpfr
        aleth #  FIXME: THIS LIB CAUSES LINKING ORDER SENSITIVITY
        )

# needed for golang runtime that comes together with taraxa-evm
if (APPLE)  # if "APPLE" not sufficient, use if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(app_base "-framework CoreFoundation")
    target_link_libraries(app_base "-framework Security")
else ()
    target_link_libraries(app_base stdc++fs)
endif ()

if(TCMALLOC_LIB)
    target_link_libraries(app_base ${TCMALLOC_LIB}) # MUST BE LINKED LAST!!!
endif()

# Main taraxad binary
add_subdirectory(taraxad)