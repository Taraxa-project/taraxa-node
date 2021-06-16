# TODO: Break this cmake(app_base target) into smaller libraries with their own cmakes, e.g. network, chain, dag, etc... could be separate libraries

set(HEADERS
        cli/config.hpp
        cli/tools.hpp

        dag/dag.hpp
        dag/dag_block_manager.hpp
        dag/dag_block.hpp
        dag/vdf_sortition.hpp
        dag/proposal_period_levels_map.hpp
        final_chain/state_api_data.hpp
        final_chain/state_api.hpp
        final_chain/trie_common.hpp
        final_chain/data.hpp
        final_chain/replay_protection_service.hpp
        final_chain/final_chain.hpp

        network/rpc/rpc_error_handler.hpp
        network/rpc/eth/watches.hpp
        network/rpc/eth/LogFilter.hpp
        network/rpc/eth/data.hpp
        network/network.hpp
        network/tarcap/taraxa_capability.hpp
        network/tarcap/stats/node_stats.hpp
        network/tarcap/threadpool/tarcap_thread_pool.hpp
        network/tarcap/threadpool/priority_queue.hpp
        network/tarcap/threadpool/packets_queue.hpp
        network/tarcap/threadpool/packet_data.hpp
        network/tarcap/threadpool/packets_blocking_mask.hpp
        network/tarcap/packet_types.hpp
        network/tarcap/shared_states/syncing_state.hpp
        network/tarcap/shared_states/test_state.hpp
        network/tarcap/shared_states/peers_state.hpp
        network/tarcap/packets_handler.hpp
        network/tarcap/taraxa_peer.hpp
        network/tarcap/packets_handlers/dag_sync_packet_handler.hpp
        network/tarcap/packets_handlers/dag_block_packet_handler.hpp
        network/tarcap/packets_handlers/get_dag_sync_packet_handler.hpp
        network/tarcap/packets_handlers/get_pbft_sync_packet_handler.hpp
        network/tarcap/packets_handlers/pbft_block_packet_handler.hpp
        network/tarcap/packets_handlers/common/packet_handler.hpp
        network/tarcap/packets_handlers/pbft_sync_packet_handler.hpp
        network/tarcap/packets_handlers/status_packet_handler.hpp
        network/tarcap/packets_handlers/test_packet_handler.hpp
        network/tarcap/packets_handlers/transaction_packet_handler.hpp
        network/tarcap/packets_handlers/vote_packets_handler.hpp
        network/tarcap/packets_handlers/common/syncing_handler.hpp
        network/tarcap/packets_handlers/common/get_blocks_request_type.hpp
        network/tarcap/stats/packets_stats.hpp
        network/tarcap/stats/packets_avg_stats.hpp
        node/full_node.hpp
        storage/db_storage.hpp
        transaction_manager/transaction_order_manager.hpp
        transaction_manager/transaction.hpp
        transaction_manager/transaction_queue.hpp
        transaction_manager/transaction_manager.hpp
        transaction_manager/transaction_status.hpp
        util/jsoncpp.hpp
        util/range_view.hpp
        util/global_const.hpp
        util/functional.hpp
        util/encoding_rlp.hpp
        util/default_construct_copyable_movable.hpp
        util/thread_pool.hpp
        util/event.hpp
        util/util.hpp
        util/lazy.hpp
        network/rpc/Net.h
        network/rpc/Taraxa.h
        network/rpc/WSServer.h
        network/rpc/Test.h
        network/rpc/EthFace.h
        network/rpc/TaraxaFace.h
        network/rpc/EthClient.h
        network/rpc/RpcServer.h
        network/rpc/eth/Eth.h
        network/rpc/NetClient.h
        network/rpc/TestClient.h
        network/rpc/TaraxaClient.h
        network/rpc/NetFace.h
        network/rpc/TestFace.h
        )

set(SOURCES
        cli/config.cpp
        cli/tools.cpp
        
        dag/dag_block_manager.cpp
        dag/dag.cpp
        dag/dag_block.cpp
        dag/proposal_period_levels_map.cpp
        dag/vdf_sortition.cpp
        final_chain/trie_common.cpp
        final_chain/final_chain.cpp
        final_chain/state_api.cpp
        final_chain/state_api_data.cpp
        final_chain/replay_protection_service.cpp
        final_chain/data.cpp

        network/rpc/Taraxa.cpp
        network/rpc/WSServer.cpp
        network/rpc/Net.cpp
        network/rpc/Test.cpp
        network/rpc/RpcServer.cpp
        network/rpc/rpc_error_handler.cpp
        network/rpc/eth/watches.cpp
        network/rpc/eth/LogFilter.cpp
        network/rpc/eth/Eth.cpp
        network/network.cpp
        network/tarcap/taraxa_capability.cpp
        network/tarcap/stats/node_stats.cpp
        network/tarcap/threadpool/tarcap_thread_pool.cpp
        network/tarcap/threadpool/priority_queue.cpp
        network/tarcap/threadpool/packets_queue.cpp
        network/tarcap/threadpool/packet_data.cpp
        network/tarcap/threadpool/packets_blocking_mask.cpp
        network/tarcap/shared_states/test_state.cpp
        network/tarcap/shared_states/peers_state.cpp
        network/tarcap/packets_handler.cpp
        network/tarcap/shared_states/syncing_state.cpp
        network/tarcap/packets_handlers/dag_sync_packet_handler.cpp
        network/tarcap/packets_handlers/dag_block_packet_handler.cpp
        network/tarcap/packets_handlers/get_dag_sync_packet_handler.cpp
        network/tarcap/packets_handlers/get_pbft_sync_packet_handler.cpp
        network/tarcap/packets_handlers/pbft_block_packet_handler.cpp
        network/tarcap/packets_handlers/common/packet_handler.cpp
        network/tarcap/packets_handlers/pbft_sync_packet_handler.cpp
        network/tarcap/packets_handlers/status_packet_handler.cpp
        network/tarcap/packets_handlers/test_packet_handler.cpp
        network/tarcap/packets_handlers/transaction_packet_handler.cpp
        network/tarcap/packets_handlers/vote_packets_handler.cpp
        network/tarcap/packets_handlers/common/syncing_handler.cpp
        network/tarcap/stats/packets_avg_stats.cpp
        network/tarcap/stats/packets_stats.cpp
        node/full_node.cpp
        storage/db_storage.cpp
        transaction_manager/transaction.cpp
        transaction_manager/transaction_queue.cpp
        transaction_manager/transaction_order_manager.cpp
        transaction_manager/transaction_manager.cpp
        util/jsoncpp.cpp
        util/util.cpp
        util/thread_pool.cpp)

add_library(app_base ${SOURCES} ${HEADERS})

add_subdirectory(network/rpc)
add_subdirectory(common)
add_subdirectory(config)
add_subdirectory(consensus)
add_subdirectory(logger)

# TODO: have some problems with include dirs for imported targets
get_target_property(TARAXA_VDF_INCUDE taraxa-vdf INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(TARAXA_VRF_INCUDE taraxa-vrf INTERFACE_INCLUDE_DIRECTORIES)

add_subdirectory(aleth)

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
        common
        config
        consensus
        logger
        taraxa-vrf
        taraxa-vdf
        taraxa-evm
        pthread
        CONAN_PKG::jsoncpp
        CONAN_PKG::libjson-rpc-cpp
        CONAN_PKG::boost
        CONAN_PKG::openssl
        CONAN_PKG::gmp
        CONAN_PKG::lz4
        CONAN_PKG::rocksdb
        CONAN_PKG::mpfr
        devcore
        devcrypto
        p2p
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

install(TARGETS app_base
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})

# Main taraxad binary
add_subdirectory(taraxad)
