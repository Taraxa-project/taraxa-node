set(HEADERS
    include/node/full_node.hpp
)

set(SOURCES
    src/full_node.cpp
)

add_library(node ${SOURCES} ${HEADERS})
target_include_directories(node PUBLIC "include")
target_link_libraries(node PRIVATE common config logger dag storage util app_base)