set(HEADERS
    include/config/version.hpp
    include/config/config.hpp
    include/config/chain_config.hpp
    include/config/final_chain_config.hpp
    include/config/pbft_config.hpp
    include/config/state_api_config.hpp
)

set(SOURCES
    src/config.cpp
    src/pbft_config.cpp
    src/chain_config.cpp
    src/final_chain_config.cpp
    src/state_api_config.cpp
)

# Configure file with version
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version/version.hpp.in ${CMAKE_CURRENT_SOURCE_DIR}/include/config/version.hpp)

add_library(config ${SOURCES} ${HEADERS})
target_include_directories(config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(config PUBLIC dag_block)

install(TARGETS common
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
