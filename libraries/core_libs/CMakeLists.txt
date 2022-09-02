add_subdirectory(network/rpc)
add_subdirectory(graphql)

file(GLOB_RECURSE CONSENSUS_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/consensus/*.hpp)
file(GLOB_RECURSE CONSENSUS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/consensus/*.cpp)

file(GLOB_RECURSE NETWORK_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/network/*.hpp)
file(GLOB_RECURSE NETWORK_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/network/*.cpp)

file(GLOB_RECURSE STORAGE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/storage/*.hpp)
file(GLOB_RECURSE STORAGE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/storage/*.cpp)

file(GLOB_RECURSE NODE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/node/*.hpp)
file(GLOB_RECURSE NODE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/node/*.cpp)

# GraphQL
file(GLOB_RECURSE GRAPHQL_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/graphql/*.hpp)
file(GLOB_RECURSE GRAPHQL_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/graphql/*.h)
file(GLOB_RECURSE GRAPHQL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/graphql/*.cpp)

set(HEADERS
    ${CONSENSUS_HEADERS}
    ${NETWORK_HEADERS}
    ${STORAGE_HEADERS}
    ${NODE_HEADERS}
    ${GRAPHQL_HEADERS}
)

set(SOURCES
    ${CONSENSUS_SOURCES}
    ${NETWORK_SOURCES}
    ${STORAGE_SOURCES}
    ${NODE_SOURCES}
    ${GRAPHQL_SOURCES}
)

add_library(core_libs ${SOURCES} ${HEADERS})
target_include_directories(core_libs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(core_libs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/consensus/include)
target_include_directories(core_libs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/network/include)
target_include_directories(core_libs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/node/include)
target_include_directories(core_libs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/storage/include)
target_include_directories(core_libs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# GraphQL
target_include_directories(core_libs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/graphql/gen)
target_include_directories(core_libs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/graphql/include)

add_dependencies(core_libs regenerate_schema_files)

target_link_libraries(core_libs PUBLIC
    cli
    pbft_block
    taraxa-evm
    p2p
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