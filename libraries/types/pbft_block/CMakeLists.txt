set(HEADERS
    include/pbft/pbft_block.hpp
    include/pbft/period_data.hpp
)
set(SOURCES
    src/pbft_block.cpp
    src/period_data.cpp
)

add_library(pbft_block ${SOURCES} ${HEADERS})
target_include_directories(pbft_block PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pbft_block PUBLIC dag_block transaction)
