
set(HEADERS
    include/vdf/config.hpp
    include/vdf/sortition.hpp
)
set(SOURCES
    src/config.cpp
    src/sortition.cpp
)

add_library(vdf ${SOURCES} ${HEADERS})
target_include_directories(vdf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${MPFR_INCLUDES})
target_link_libraries(vdf PUBLIC
    logger
    taraxa-vdf
    ${MPFR_LIBRARIES}
)

install(TARGETS vdf
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)