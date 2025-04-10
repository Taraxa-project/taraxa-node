set(HEADERS
    include/app/app.hpp
)

set(SOURCES
    src/app.cpp
)

add_library(app ${SOURCES} ${HEADERS})
target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(app PUBLIC
    common
    core_libs
    rpc_plugin
)

install(TARGETS app
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)