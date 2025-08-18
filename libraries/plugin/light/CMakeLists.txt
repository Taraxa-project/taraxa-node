set(HEADERS
    include/plugin/light.hpp
)

set(SOURCES
    src/light.cpp
)

add_library(light_plugin ${SOURCES} ${HEADERS})
target_include_directories(light_plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(light_plugin PUBLIC
    common
    core_libs
    plugin
)

install(TARGETS light_plugin
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)