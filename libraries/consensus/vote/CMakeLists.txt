set(HEADERS
    include/vote/vote.hpp
)

set(SOURCES
    src/vote.cpp
)

add_library(vote STATIC ${SOURCES} ${HEADERS})
target_include_directories(vote PUBLIC "include")
target_link_libraries(vote PRIVATE 
    common
    config
    dag
    pbft
    dag_block
    final_chain
    logger
    network
    storage
    transaction_manager
    aleth
    vdf
    taraxa-vrf
    taraxa-evm
    CONAN_PKG::boost
    CONAN_PKG::jsoncpp
)
