set(HEADERS
    include/storage/db_storage.hpp
)

set(SOURCES
    src/db_storage.cpp
)

add_library(storage STATIC ${SOURCES} ${HEADERS})
target_include_directories(storage PUBLIC "include")
target_link_libraries(storage PRIVATE 
    common
    config
    consensus
    dag
    final_chain
    logger
    util
    node
    network
    transaction_manager
    aleth
    taraxa-evm
    taraxa-vrf
    CONAN_PKG::jsoncpp
    CONAN_PKG::libjson-rpc-cpp
    CONAN_PKG::rocksdb
)