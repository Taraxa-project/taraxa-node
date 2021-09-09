set(HEADERS include/dag/proposal_period_levels_map.hpp)
set(SOURCES src/proposal_period_levels_map.cpp)

add_library(proposal_period_levels_map ${SOURCES} ${HEADERS})
target_include_directories(proposal_period_levels_map PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(proposal_period_levels_map PRIVATE 
    common
)
