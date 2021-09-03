set(HEADERS
    include/node/full_node.hpp
)

set(SOURCES
    src/full_node.cpp
)

add_library(node STATIC ${SOURCES} ${HEADERS})
target_include_directories(node PUBLIC "include")
target_link_libraries(node PRIVATE
    common
    config
    consensus
    final_chain
    logger
    network
    storage
    taraxa-vrf
    transaction_manager
    aleth
    taraxa-evm
    CONAN_PKG::boost
    CONAN_PKG::jsoncpp
    CONAN_PKG::libjson-rpc-cpp
)