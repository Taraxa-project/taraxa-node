file(GLOB sources "*.cpp" "*.h")

add_library(p2p ${sources})
target_link_libraries(p2p PUBLIC devcrypto CONAN_PKG::lz4)

target_include_directories(p2p SYSTEM PUBLIC ../)

if(MINIUPNPC)
    target_link_libraries(p2p PUBLIC "${MINIUPNPC_LIBRARY}")
    target_include_directories(p2p SYSTEM PUBLIC "${MINIUPNPC_INCLUDE_DIR}")
    target_compile_definitions(p2p PUBLIC ETH_MINIUPNPC)
endif()