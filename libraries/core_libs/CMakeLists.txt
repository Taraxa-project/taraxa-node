add_subdirectory(network/rpc)

file(GLOB_RECURSE CONSENSUS_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/consensus/*.hpp)
file(GLOB_RECURSE CONSENSUS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/consensus/*.cpp)

file(GLOB_RECURSE NETWORK_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/network/*.hpp)
file(GLOB_RECURSE NETWORK_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/network/*.cpp)
# GraphQL
include(${PROJECT_SOURCE_DIR}/CMakeModules/cpp_graphql_gen.cmake)
file(GLOB_RECURSE NETWORK_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/network/*.h)

file(GLOB_RECURSE STORAGE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/storage/*.hpp)
file(GLOB_RECURSE STORAGE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/storage/*.cpp)

file(GLOB_RECURSE NODE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/node/*.hpp)
file(GLOB_RECURSE NODE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/node/*.cpp)

set(HEADERS
    ${CONSENSUS_HEADERS}
    ${NETWORK_HEADERS}
    ${STORAGE_HEADERS}
    ${NODE_HEADERS}
)

set(SOURCES
    ${CONSENSUS_SOURCES}
    ${NETWORK_SOURCES}
    ${STORAGE_SOURCES}
    ${NODE_SOURCES}
    ${GRAPHQL_GENERATED_SOURCES}
)

add_library(core_libs ${SOURCES} ${HEADERS})
target_include_directories(core_libs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(core_libs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/consensus/include)
target_include_directories(core_libs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/network/include)
target_include_directories(core_libs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/node/include)
target_include_directories(core_libs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/storage/include)
target_include_directories(core_libs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# GraphQL
target_include_directories(core_libs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/network/graphql/gen)
target_include_directories(core_libs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/network/graphql/include)

target_link_libraries(core_libs PUBLIC
    cli
    pbft_block
    taraxa-evm
    p2p
    metrics
    CONAN_PKG::libjson-rpc-cpp
    CONAN_PKG::rocksdb
# GraphQL
    cppgraphqlgen::graphqlservice
    cppgraphqlgen::graphqljson
)


install(TARGETS core_libs
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)