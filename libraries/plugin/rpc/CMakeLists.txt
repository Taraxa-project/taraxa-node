set(HEADERS
    include/plugin/rpc.hpp
)

set(SOURCES
    src/rpc.cpp
)

add_library(rpc_plugin ${SOURCES} ${HEADERS})
target_include_directories(rpc_plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(rpc_plugin PUBLIC
    common
    core_libs
    plugin
)

install(TARGETS rpc_plugin
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)