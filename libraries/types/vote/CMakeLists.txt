file(GLOB_RECURSE VOTE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)
file(GLOB_RECURSE VOTE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(vote ${VOTE_SOURCES} ${VOTE_HEADERS})
target_include_directories(vote PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(Boost REQUIRED)
target_link_libraries(vote PUBLIC common devcrypto Boost::boost mpfr::mpfr)

