set(HEADERS
    include/consensus/pbft_chain.hpp
    include/consensus/block_proposer.hpp
    include/consensus/vote.hpp
    include/consensus/pbft_manager.hpp
    include/consensus/sync_block.hpp
    include/consensus/sync_queue.hpp

    include/dag/dag_block_manager.hpp
    include/dag/dag_block.hpp
    include/dag/dag.hpp
    include/dag/proposal_period_levels_map.hpp
    include/dag/vdf_sortition.hpp
)

set(SOURCES
    src/pbft_manager.cpp
    src/vote.cpp
    src/pbft_chain.cpp
    src/block_proposer.cpp
    src/sync_block.cpp
    src/sync_queue.cpp

    src/dag_block_manager.cpp
    src/dag_block.cpp
    src/dag.cpp
    src/proposal_period_levels_map.cpp
    src/vdf_sortition.cpp
)

add_library(consensus ${SOURCES} ${HEADERS})
target_include_directories(consensus PUBLIC "include")
target_link_libraries(consensus PUBLIC
    taraxa-vdf
    CONAN_PKG::mpfr
)
target_link_libraries(consensus PRIVATE 
    common
    config
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
    CONAN_PKG::libjson-rpc-cpp
)
