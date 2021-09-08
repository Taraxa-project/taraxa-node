set(HEADERS
    include/cli/config.hpp
    include/cli/default_config.hpp
    include/cli/devnet_config.hpp
    include/cli/testnet_config.hpp
    include/cli/tools.hpp
)

set(SOURCES
    src/config.cpp
    src/tools.cpp
)

add_library(cli ${SOURCES} ${HEADERS})
target_include_directories(cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(cli PRIVATE 
    config
    common
    consensus
    logger
    aleth
    vdf
    taraxa-vrf
    CONAN_PKG::jsoncpp 
    CONAN_PKG::boost
)

install(TARGETS cli
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)