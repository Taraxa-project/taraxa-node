
set(HEADERS include/vdf/sortition.hpp include/vdf/config.hpp)
set(SOURCES src/sortition.cpp src/config.cpp)

add_library(vdf ${SOURCES} ${HEADERS})
target_include_directories(vdf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(vdf PUBLIC
    common
    logger
    taraxa-vdf
    CONAN_PKG::mpfr
)

install(TARGETS vdf
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)