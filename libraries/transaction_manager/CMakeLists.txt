set(HEADERS
    include/transaction_manager/transaction_manager.hpp
    include/transaction_manager/transaction_queue.hpp
    include/transaction_manager/transaction_status.hpp
    include/transaction_manager/transaction.hpp
)

set(SOURCES
    src/transaction_manager.cpp
    src/transaction_queue.cpp
    src/transaction.cpp
)

add_library(transaction_manager STATIC ${SOURCES} ${HEADERS})
target_include_directories(transaction_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(transaction_manager PUBLIC CONAN_PKG::rocksdb)
target_link_libraries(transaction_manager PRIVATE 
    common
    config
    consensus
    final_chain 
    logger
    storage
    aleth
    vdf
    taraxa-evm
    taraxa-vrf
    CONAN_PKG::jsoncpp
    CONAN_PKG::libjson-rpc-cpp
)