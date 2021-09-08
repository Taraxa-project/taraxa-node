set(HEADERS
    include/dag/dag_block_manager.hpp
    include/dag/dag.hpp
    include/dag/proposal_period_levels_map.hpp
)

set(SOURCES
    src/dag_block_manager.cpp
    src/dag.cpp
    src/proposal_period_levels_map.cpp
)

add_library(dag STATIC ${SOURCES} ${HEADERS})
target_include_directories(dag PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(dag PUBLIC dag_block) 
target_link_libraries(dag PRIVATE 
    common
    config
    dag_block
    final_chain
    logger
    network
    storage
    pbft
    transaction_manager
    aleth
    vote
    vdf
    taraxa-vrf
    taraxa-evm
    CONAN_PKG::boost
    CONAN_PKG::jsoncpp
    CONAN_PKG::libjson-rpc-cpp
)
