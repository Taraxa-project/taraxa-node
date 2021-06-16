set(HEADERS
    include/dag/dag_block_manager.hpp
    include/dag/dag_block.hpp
    include/dag/dag.hpp
    include/dag/proposal_period_levels_map.hpp
    include/dag/vdf_sortition.hpp
)

set(SOURCES
    src/dag_block_manager.cpp
    src/dag_block.cpp
    src/dag.cpp
    src/proposal_period_levels_map.cpp
    src/vdf_sortition.cpp
)

add_library(dag ${SOURCES} ${HEADERS})
target_include_directories(dag PUBLIC "include")
target_link_libraries(dag PRIVATE common config logger util app_base)