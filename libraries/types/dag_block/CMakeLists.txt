set(HEADERS
    include/dag/dag_block.hpp
)
set(SOURCES
    src/dag_block.cpp
)

add_library(dag_block ${SOURCES} ${HEADERS})
target_include_directories(dag_block PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(dag_block PUBLIC devcrypto vdf)