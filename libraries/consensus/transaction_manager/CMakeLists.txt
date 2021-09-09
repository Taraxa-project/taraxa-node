set(HEADERS
    include/transaction_manager/transaction_manager.hpp
    include/transaction_manager/transaction_queue.hpp
)

set(SOURCES
    src/transaction_manager.cpp
    src/transaction_queue.cpp
)

add_library(transaction_manager STATIC ${SOURCES} ${HEADERS})
target_include_directories(transaction_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(transaction_manager PUBLIC transaction)
target_link_libraries(transaction_manager PRIVATE 
    common
    config
    dag
    final_chain 
    logger
    pbft
    storage
    vote
)