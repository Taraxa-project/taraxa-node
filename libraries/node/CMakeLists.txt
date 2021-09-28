file(GLOB_RECURSE CONSENSUS_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/consensus/*.hpp)
file(GLOB_RECURSE CONSENSUS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/consensus/*.cpp)

file(GLOB_RECURSE NETWORK_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/network/*.hpp)
file(GLOB_RECURSE NETWORK_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/network/*.cpp)

set(HEADERS
    ${CONSENSUS_HEADERS}
    ${NETWORK_HEADERS}
    include/node/node.hpp
)

set(SOURCES
    ${CONSENSUS_SOURCES}
    ${NETWORK_SOURCES}
    src/node.cpp
)

add_library(node ${SOURCES} ${HEADERS})
target_include_directories(node PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(node PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/network/include)
target_include_directories(node PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/consensus/include)
target_include_directories(node PUBLIC "./")
target_link_libraries(node PUBLIC 
    dag_block 
    pbft_block 
    proposal_period_levels_map 
    transaction 
    vote
    taraxa-evm
    CONAN_PKG::libjson-rpc-cpp
)
target_link_libraries(node PRIVATE
    common
    cli
    config
    logger
    storage
)

if (APPLE)  # if "APPLE" not sufficient, use if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(node PUBLIC "-framework CoreFoundation")
    target_link_libraries(node PUBLIC "-framework Security")
else ()
    target_link_libraries(node PUBLIC stdc++fs)
endif ()

if(TCMALLOC_LIB)
    target_link_libraries(node PUBLIC ${TCMALLOC_LIB}) # MUST BE LINKED LAST!!!
endif()

install(TARGETS node
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)