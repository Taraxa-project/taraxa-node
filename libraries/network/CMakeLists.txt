set(HEADERS
    rpc/rpc_error_handler.hpp
    rpc/eth/watches.hpp
    rpc/eth/LogFilter.hpp
    rpc/eth/data.hpp
    rpc/eth/Eth.h
    rpc/eth/LogFilter.hpp
    rpc/eth/watches.hpp

    rpc/EthClient.h
    rpc/EthFace.h
    rpc/Net.h
    rpc/NetClient.h
    rpc/NetFace.h
    rpc/rpc_error_handler.hpp
    rpc/RpcServer.h
    rpc/Taraxa.h
    rpc/TaraxaClient.h
    rpc/TaraxaFace.h
    rpc/Test.h
    rpc/TestClient.h
    rpc/TestFace.h

    include/network/network.hpp
    include/network/network.hpp
    include/network/tarcap/taraxa_capability.hpp
    include/network/tarcap/packets_handler.hpp
    include/network/tarcap/packet_types.hpp
    include/network/tarcap/taraxa_peer.hpp
    include/network/tarcap/threadpool/tarcap_thread_pool.hpp
    include/network/tarcap/threadpool/priority_queue.hpp
    include/network/tarcap/threadpool/packets_queue.hpp
    include/network/tarcap/threadpool/packet_data.hpp
    include/network/tarcap/threadpool/packets_blocking_mask.hpp
    include/network/tarcap/packets_handlers/common/packet_handler.hpp
    include/network/tarcap/packets_handlers/common/syncing_handler.hpp
    include/network/tarcap/packets_handlers/common/get_blocks_request_type.hpp
    include/network/tarcap/packets_handlers/dag_block_packet_handler.hpp
    include/network/tarcap/packets_handlers/dag_sync_packet_handler.hpp
    include/network/tarcap/packets_handlers/get_dag_sync_packet_handler.hpp
    include/network/tarcap/packets_handlers/get_pbft_sync_packet_handler.hpp
    include/network/tarcap/packets_handlers/pbft_block_packet_handler.hpp
    include/network/tarcap/packets_handlers/pbft_sync_packet_handler.hpp
    include/network/tarcap/packets_handlers/status_packet_handler.hpp
    include/network/tarcap/packets_handlers/test_packet_handler.hpp
    include/network/tarcap/packets_handlers/transaction_packet_handler.hpp
    include/network/tarcap/packets_handlers/vote_packets_handler.hpp
    include/network/tarcap/shared_states/syncing_state.hpp
    include/network/tarcap/shared_states/test_state.hpp
    include/network/tarcap/shared_states/peers_state.hpp
    include/network/tarcap/stats/node_stats.hpp
    include/network/tarcap/stats/packets_stats.hpp
    include/network/tarcap/stats/packets_avg_stats.hpp
)

set(SOURCES
    rpc/eth/Eth.cpp
    rpc/eth/LogFilter.cpp
    rpc/eth/watches.cpp

    rpc/Net.cpp
    rpc/rpc_error_handler.cpp
    rpc/RpcServer.cpp
    rpc/Taraxa.cpp
    rpc/Test.cpp
    rpc/WSServer.cpp
    
    src/network.cpp

    src/tarcap/taraxa_capability.cpp
    src/tarcap/packets_handler.cpp
    src/tarcap/threadpool/tarcap_thread_pool.cpp
    src/tarcap/threadpool/priority_queue.cpp
    src/tarcap/threadpool/packets_queue.cpp
    src/tarcap/threadpool/packet_data.cpp
    src/tarcap/threadpool/packets_blocking_mask.cpp
    src/tarcap/packets_handlers/common/packet_handler.cpp
    src/tarcap/packets_handlers/common/syncing_handler.cpp
    src/tarcap/packets_handlers/dag_block_packet_handler.cpp
    src/tarcap/packets_handlers/dag_sync_packet_handler.cpp
    src/tarcap/packets_handlers/get_dag_sync_packet_handler.cpp
    src/tarcap/packets_handlers/get_pbft_sync_packet_handler.cpp
    src/tarcap/packets_handlers/pbft_block_packet_handler.cpp
    src/tarcap/packets_handlers/pbft_sync_packet_handler.cpp
    src/tarcap/packets_handlers/status_packet_handler.cpp
    src/tarcap/packets_handlers/test_packet_handler.cpp
    src/tarcap/packets_handlers/transaction_packet_handler.cpp
    src/tarcap/packets_handlers/vote_packets_handler.cpp
    src/tarcap/shared_states/test_state.cpp
    src/tarcap/shared_states/peers_state.cpp
    src/tarcap/shared_states/syncing_state.cpp
    src/tarcap/stats/packets_stats.cpp
    src/tarcap/stats/packets_avg_stats.cpp
    src/tarcap/stats/node_stats.cpp
)

add_library(network STATIC ${SOURCES} ${HEADERS})
target_include_directories(network PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(network PUBLIC "../")
target_link_libraries(network PRIVATE
    common
    config
    consensus
    final_chain
    logger
    node
    storage
    transaction_manager
    aleth
    vdf
    taraxa-evm
    taraxa-vrf
    CONAN_PKG::boost
    CONAN_PKG::jsoncpp
    CONAN_PKG::libjson-rpc-cpp
)

add_subdirectory(rpc)