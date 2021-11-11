set(HEADERS include/vote/vote.hpp)
set(SOURCES src/vote.cpp)

add_library(vote ${SOURCES} ${HEADERS})
target_include_directories(vote PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(vote PUBLIC CONAN_PKG::boost CONAN_PKG::mpfr devcrypto)
target_link_libraries(vote PRIVATE common)
