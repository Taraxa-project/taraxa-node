set(HEADERS
    include/consensus/pbft_chain.hpp
    include/consensus/block_proposer.hpp
    include/consensus/vrf_wrapper.hpp
    include/consensus/vote.hpp
    include/consensus/pbft_manager.hpp
    include/consensus/sync_block.hpp
    include/consensus/sync_queue.hpp
)

set(SOURCES
    src/pbft_manager.cpp
    src/vrf_wrapper.cpp
    src/vote.cpp
    src/pbft_chain.cpp
    src/block_proposer.cpp
    src/sync_block.cpp
    src/sync_queue.cpp
)

add_library(consensus STATIC ${SOURCES} ${HEADERS})
target_include_directories(consensus PUBLIC "include")
target_link_libraries(consensus PRIVATE 
    common
    config
    dag
    final_chain
    logger
    network
    storage
    taraxa-vrf
    transaction_manager
    aleth
    taraxa-evm
    CONAN_PKG::boost
    CONAN_PKG::jsoncpp
)
