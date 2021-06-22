set(HEADERS
    include/storage/db_storage.hpp
)

set(SOURCES
    src/db_storage.cpp
)

add_library(storage ${SOURCES} ${HEADERS})
target_include_directories(storage PUBLIC "include")
target_link_libraries(storage PRIVATE 
    common
    consensus
    logger
    dag
    util
    node
    aleth
    taraxa-evm
    CONAN_PKG::jsoncpp
    CONAN_PKG::libjson-rpc-cpp
    CONAN_PKG::rocksdb
)