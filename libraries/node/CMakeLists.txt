set(HEADERS
    include/node/node.hpp
)

set(SOURCES
    src/node.cpp
)

add_library(node STATIC ${SOURCES} ${HEADERS})
target_include_directories(node PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(node PUBLIC CONAN_PKG::libjson-rpc-cpp)
target_link_libraries(node PRIVATE
    common
    config
    consensus
    logger
    network
    storage
)