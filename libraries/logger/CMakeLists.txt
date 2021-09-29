set(HEADERS
    include/logger/logger.hpp
    include/logger/logger_config.hpp
)

set(SOURCES
    src/logger.cpp
    src/logger_config.cpp
)

add_library(logger ${SOURCES} ${HEADERS})
target_include_directories(logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(logger PRIVATE common)

install(TARGETS logger
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)