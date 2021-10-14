set(HEADERS 
    include/transaction/transaction.hpp
)
set(SOURCES src/transaction.cpp)

add_library(transaction ${SOURCES} ${HEADERS})
target_include_directories(transaction PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(transaction PRIVATE common vote)
