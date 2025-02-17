set(HEADERS
    include/transaction/transaction.hpp
    include/transaction/receipt.hpp
    include/transaction/system_transaction.hpp
)
set(SOURCES src/transaction.cpp  src/receipt.cpp src/system_transaction.cpp)

add_library(transaction ${SOURCES} ${HEADERS})
target_include_directories(transaction PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(transaction PUBLIC vote)
