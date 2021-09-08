set(HEADERS
    include/pbft/block_proposer.hpp
    include/pbft/chain.hpp
    include/pbft/manager.hpp
    include/pbft/sync_block.hpp
    include/pbft/sync_queue.hpp
)

set(SOURCES
    src/block_proposer.cpp
    src/chain.cpp
    src/manager.cpp
    src/sync_block.cpp
    src/sync_queue.cpp
)

add_library(pbft STATIC ${SOURCES} ${HEADERS})
target_include_directories(pbft PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pbft PRIVATE 
    common
    config
    dag
    dag_block
    final_chain
    logger
    network
    storage
    vote
    transaction_manager
    aleth
    vdf
    taraxa-vrf
    taraxa-evm
    CONAN_PKG::boost
    CONAN_PKG::jsoncpp
)
