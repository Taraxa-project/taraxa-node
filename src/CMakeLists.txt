# TODO: Break this cmake(app_base target) into smaller libraries with their own cmakes, e.g. network, chain, dag, etc... could be separate libraries

set(SOURCES
        common/types.cpp
        config/chain_config.cpp
        config/config.cpp
        consensus/pbft_manager.cpp
        consensus/vote.cpp
        consensus/pbft_config.cpp
        consensus/block_proposer.cpp
        consensus/vrf_wrapper.cpp
        consensus/pbft_chain.cpp
        dag/dag_block_manager.cpp
        dag/dag.cpp
        dag/dag_block.cpp
        dag/proposal_period_levels_map.cpp
        dag/vdf_sortition.cpp
        final_chain/TrieCommon.cpp
        final_chain/data.cpp
        final_chain/final_chain.cpp
        final_chain/state_api.cpp
        final_chain/state_api_data.cpp
        final_chain/replay_protection_service.cpp
        logger/log.cpp
        logger/logger_config.cpp
        network/rpc/Taraxa.cpp
        network/rpc/WSServer.cpp
        network/rpc/Net.cpp
        network/rpc/Test.cpp
        network/rpc/RpcServer.cpp
        network/rpc/rpc_error_handler.cpp
        network/rpc/eth/impl/Eth.cpp
        network/network.cpp
        network/packets_stats.cpp
        network/taraxa_capability.cpp
        node/full_node.cpp
        storage/db_storage.cpp
        transaction_manager/transaction.cpp
        transaction_manager/transaction_queue.cpp
        transaction_manager/transaction_order_manager.cpp
        transaction_manager/transaction_manager.cpp
        util/jsoncpp.cpp
        util/util.cpp
        util/thread_pool.cpp
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
        scrypt
        CONAN_PKG::jsoncpp
        CONAN_PKG::libjson-rpc-cpp
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

if (TCMALLOC_LIB)
    target_link_libraries(app_base ${TCMALLOC_LIB}) # MUST BE LINKED LAST!!!
endif ()

# Main taraxad binary
add_subdirectory(taraxad)