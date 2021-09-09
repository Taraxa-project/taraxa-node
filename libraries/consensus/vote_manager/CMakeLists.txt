set(HEADERS
    include/vote/vote_manager.hpp
)

set(SOURCES
    src/vote_manager.cpp
)

add_library(vote_manager STATIC ${SOURCES} ${HEADERS})
target_include_directories(vote_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(vote_manager PUBLIC vote) 
target_link_libraries(vote_manager PRIVATE 
    common
    config
    dag
    pbft
    final_chain
    logger
    network
    storage
    transaction_manager
)
