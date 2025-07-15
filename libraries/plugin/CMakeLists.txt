set(HEADERS
    include/plugin/plugin.hpp
)

set(SOURCES
)

add_library(plugin INTERFACE ${HEADERS})
target_include_directories(plugin INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(plugin INTERFACE
    common
)

install(TARGETS plugin
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

add_subdirectory(rpc)
add_subdirectory(light)