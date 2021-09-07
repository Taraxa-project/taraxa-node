set(HEADERS include/dag/dag_block.hpp)
set(SOURCES src/dag_block.cpp)

add_library(dag_block ${SOURCES} ${HEADERS})
target_include_directories(dag_block PUBLIC "include")
target_link_libraries(dag_block PUBLIC vdf)
target_link_libraries(dag_block PRIVATE 
    common
    logger
    aleth
)
