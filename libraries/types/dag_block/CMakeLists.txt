set(HEADERS
    include/dag/dag_block.hpp
    include/dag/dag_block_bundle_rlp.hpp
)
set(SOURCES
    src/dag_block.cpp
    src/dag_block_bundle_rlp.cpp
)

add_library(dag_block ${SOURCES} ${HEADERS})
target_include_directories(dag_block PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(dag_block PUBLIC devcrypto vdf)