set(HEADERS 
    include/pbft/pbft_block.hpp
    include/pbft/sync_block.hpp
)
set(SOURCES 
    src/pbft_block.cpp
    src/sync_block.cpp
)

add_library(pbft_block ${SOURCES} ${HEADERS})
target_include_directories(pbft_block PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pbft_block PUBLIC dag_block transaction)
target_link_libraries(pbft_block PRIVATE common vote)
