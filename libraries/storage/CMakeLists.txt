set(HEADERS
    include/storage/db_storage.hpp
)

set(SOURCES
    src/db_storage.cpp
)

add_library(storage STATIC ${SOURCES} ${HEADERS})
target_include_directories(storage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(storage PRIVATE 
    common
    config
    consensus
    final_chain
    logger
    network
    transaction_manager
    aleth
    vdf
    taraxa-evm
    taraxa-vrf
    CONAN_PKG::jsoncpp
    CONAN_PKG::libjson-rpc-cpp
    CONAN_PKG::rocksdb
)