file(GLOB sources "*.cpp" "*.h")

add_library(p2p ${sources})
find_package(lz4 REQUIRED)
target_link_libraries(p2p PUBLIC devcrypto LZ4::lz4_static logger)
target_include_directories(p2p SYSTEM PUBLIC ../ ${LZ4_INCLUDE_DIR})

if(MINIUPNPC)
    target_link_libraries(p2p PUBLIC "${MINIUPNPC_LIBRARY}")
    target_include_directories(p2p SYSTEM PUBLIC "${MINIUPNPC_INCLUDE_DIR}")
    target_compile_definitions(p2p PUBLIC ETH_MINIUPNPC)
endif()