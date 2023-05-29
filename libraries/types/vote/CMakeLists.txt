set(HEADERS include/vote/vote.hpp include/vote/vrf_sortition.hpp include/vote/votes_bundle_rlp.hpp)
set(SOURCES src/vote.cpp src/vrf_sortition.cpp src/votes_bundle_rlp.cpp)

add_library(vote ${SOURCES} ${HEADERS})
target_include_directories(vote PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(vote PUBLIC common devcrypto CONAN_PKG::boost)

