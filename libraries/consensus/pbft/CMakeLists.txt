set(HEADERS
    include/pbft/block_proposer.hpp
    include/pbft/pbft_chain.hpp
    include/pbft/pbft_manager.hpp
    include/pbft/sync_queue.hpp
)

set(SOURCES
    src/block_proposer.cpp
    src/pbft_chain.cpp
    src/pbft_manager.cpp
    src/sync_queue.cpp
)

add_library(pbft STATIC ${SOURCES} ${HEADERS})
target_include_directories(pbft PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pbft PUBLIC pbft_block) 
target_link_libraries(pbft PRIVATE 
    common
    config
    dag
    final_chain
    logger
    network
    storage
    transaction_manager
    vote_manager
)
