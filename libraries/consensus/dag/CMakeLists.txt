set(HEADERS
    include/dag/dag_block_manager.hpp
    include/dag/dag.hpp
)

set(SOURCES
    src/dag_block_manager.cpp
    src/dag.cpp
)

add_library(dag STATIC ${SOURCES} ${HEADERS})
target_include_directories(dag PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(dag PUBLIC dag_block proposal_period_levels_map) 
target_link_libraries(dag PRIVATE 
    common
    config
    final_chain
    logger
    network
    storage
    pbft
    transaction_manager
    vote
    CONAN_PKG::jsoncpp
    CONAN_PKG::libjson-rpc-cpp
)
