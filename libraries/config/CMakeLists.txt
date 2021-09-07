set(HEADERS
    include/config/version.hpp
    include/config/config.hpp
    include/config/pbft.hpp
    include/config/chain.hpp
    include/config/final_chain.hpp
    include/config/state_api.hpp
)

set(SOURCES
    src/config.cpp
    src/pbft.cpp
    src/chain.cpp
    src/final_chain.cpp
    src/state_api.cpp
)

# Configure file with version
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version/version.hpp.in ${CMAKE_CURRENT_SOURCE_DIR}/include/config/version.hpp)

add_library(config ${SOURCES} ${HEADERS})
target_include_directories(config PUBLIC "include")
target_link_libraries(config PRIVATE 
    common
    dag_block
    logger
    aleth
    vdf
    taraxa-vrf
    CONAN_PKG::jsoncpp
)
