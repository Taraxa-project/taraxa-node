set(HEADERS
    include/consensus/pbft_chain.hpp
    include/consensus/block_proposer.hpp
    include/consensus/vrf_wrapper.hpp
    include/consensus/vote.hpp
    include/consensus/pbft_manager.hpp
)

set(SOURCES
    src/pbft_manager.cpp
    src/vrf_wrapper.cpp
    src/vote.cpp
    src/pbft_chain.cpp
    src/block_proposer.cpp
)

add_library(consensus STATIC ${SOURCES} ${HEADERS})
target_include_directories(consensus PUBLIC "include")
target_link_libraries(consensus PUBLIC 
    common
    config
    dag
    final_chain
    logger
    network
    storage
    taraxa-vrf
    transaction_manager
    util
    aleth
    taraxa-evm
    CONAN_PKG::boost
    CONAN_PKG::jsoncpp
)
